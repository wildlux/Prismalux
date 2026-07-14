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
#include <QInputDialog>
#include <QToolTip>
#include <QCursor>
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

/* Range di indirizzi virtuali da escludere (libvirt, VirtualBox, WSL, Docker) */
static bool isVirtualIp(const QString& s)
{
    static const char* kVirtPfx[] = {
        "192.168.122.", /* libvirt NAT default */
        "192.168.56.",  /* VirtualBox host-only */
        "192.168.49.",  /* minikube/Android USB reverse */
        "192.168.100.", /* WSL2 virtual */
        "172.17.",      /* Docker bridge */
        "172.18.",      /* Docker bridge */
        "172.19.",      /* Docker bridge */
        nullptr
    };
    for (int i = 0; kVirtPfx[i]; ++i)
        if (s.startsWith(QLatin1String(kVirtPfx[i]))) return true;
    return false;
}

QString LanWanPage::autoDetectIp() const
{
    QString fallback192, fallback10;
    for (const QNetworkInterface& iface : QNetworkInterface::allInterfaces()) {
        if (iface.flags().testFlag(QNetworkInterface::IsLoopBack)) continue;
        if (!iface.flags().testFlag(QNetworkInterface::IsUp))      continue;
        for (const QNetworkAddressEntry& e : iface.addressEntries()) {
            if (e.ip().protocol() != QAbstractSocket::IPv4Protocol) continue;
            const QString s = e.ip().toString();
            if (s.startsWith("192.168.") && !isVirtualIp(s) && fallback192.isEmpty())
                fallback192 = s;
            if ((s.startsWith("10.") || s.startsWith("172.")) && !isVirtualIp(s)
                && fallback10.isEmpty())
                fallback10 = s;
        }
    }
    if (!fallback192.isEmpty()) return fallback192;
    if (!fallback10.isEmpty())  return fallback10;
    return QStringLiteral("127.0.0.1");
}

QString LanWanPage::localLanIp() const
{
    /* Se gli spinbox sono inizializzati, usa i loro valori */
    if (m_lanIpOct[0])
        return QString("%1.%2.%3.%4")
            .arg(m_lanIpOct[0]->value())
            .arg(m_lanIpOct[1]->value())
            .arg(m_lanIpOct[2]->value())
            .arg(m_lanIpOct[3]->value());
    return autoDetectIp();
}

QString LanWanPage::serverScheme() const
{
    return (m_lanServer && m_lanServer->isTlsEnabled()) ? "https" : "http";
}

void LanWanPage::addExtraTab(QWidget* w, const QString& label)
{
    if (m_tabs) m_tabs->addTab(w, label);
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

    auto* hdr = new QLabel(tr("<b>") + subtitle + "</b>", dlg);
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

    auto* copyBtn = new QPushButton(tr("\xf0\x9f\x93\x8b" "  Copia URL"), dlg);
    connect(copyBtn, &QPushButton::clicked, dlg, [url, copyBtn]() {
        QApplication::clipboard()->setText(url);
        copyBtn->setText(tr("\xe2\x9c\x85" "  Copiato!"));
    });
    vl->addWidget(copyBtn);

    if (!note.isEmpty()) {
        auto* noteLbl2 = new QLabel(tr("<small><i>") + note + "</i></small>", dlg);
        noteLbl2->setTextFormat(Qt::RichText);
        noteLbl2->setAlignment(Qt::AlignCenter);
        noteLbl2->setWordWrap(true);
        vl->addWidget(noteLbl2);
    }

    /* OS-7: certificato self-signed — senza questa riga l'avviso del browser
       al primo accesso sembra un errore e l'utente abbandona. */
    if (url.startsWith("https://", Qt::CaseInsensitive)) {
        auto* tlsLbl = new QLabel(
            tr("<small>\xf0\x9f\x94\x92 Il browser mostrer\xc3\xa0 un avviso di sicurezza: "
               "\xc3\xa8 normale (certificato self-signed). "
               "Tocca <b>Avanzate \xe2\x86\x92 Procedi</b> per continuare.</small>"), dlg);
        tlsLbl->setTextFormat(Qt::RichText);
        tlsLbl->setAlignment(Qt::AlignCenter);
        tlsLbl->setWordWrap(true);
        vl->addWidget(tlsLbl);
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

    m_tabs = new QTabWidget(this);
    auto* tabs = m_tabs;
    tabs->addTab(buildLanAndroidTab(), "\xf0\x9f\x93\xb1  LAN Android");  /* 📱 */
    tabs->addTab(buildGNS3Tab(),       "\xf0\x9f\x8c\x90  GNS3 MCP");     /* 🌐 */

    /* onModelsReady rimosso: ModelComboBox gestisce il fetch autonomamente */

    tabs->addTab(buildWanComputeTab(), "\xf0\x9f\x96\xa7  WAN Compute");
    tabs->addTab(new SshManagerWidget(this), "\xf0\x9f\x94\x91  SSH");  /* 🔑 */

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

    /* URL formato https://IP:PORT/web?token=TOKEN (schema reale del server:
       con TLS attivo un link http:// non risponde affatto — connessione rifiutata)
       - È un link reale: si apre nel browser del telefono con la web chat già autenticata
       - L'app Android lo scansiona e compila IP, porta e token automaticamente */
    QString url = QString("%1://%2:%3/web").arg(serverScheme()).arg(ip).arg(port);
    if (!token.isEmpty()) {
        const QString enc = QString::fromLatin1(QUrl::toPercentEncoding(token));
        url += "?token=" + enc;
    }

    const bool hasTls = m_lanServer && m_lanServer->isTlsEnabled();
    QString note;
    if (token.isEmpty()) {
        note = "\xf0\x9f\x93\xb1" "  Impostazioni \xe2\x86\x92 \xf0\x9f\x93\xb7 Scansiona QR<br>"
               "<small>Nessun token \xe2\x80\x94 aggiungi un token per la sicurezza</small>";
    } else if (!hasTls) {
        note = "\xf0\x9f\x93\xb1" "  Impostazioni \xe2\x86\x92 \xf0\x9f\x93\xb7 Scansiona QR<br>"
               "<small>IP + porta + token inclusi nel QR</small><br>"
               "<small style='color:#f59e0b;'>"
               "\xe2\x9a\xa0\xef\xb8\x8f" "  TLS non attivo \xe2\x80\x94 il token viaggia in chiaro sulla rete."
               " Abilita TLS nelle impostazioni server.</small>";
    } else {
        note = "\xf0\x9f\x93\xb1" "  Impostazioni \xe2\x86\x92 \xf0\x9f\x93\xb7 Scansiona QR<br>"
               "<small>IP + porta + token inclusi nel QR \xe2\x80\x94 "
               "\xf0\x9f\x94\x92 TLS attivo</small>";
    }
    if (!token.isEmpty()) {
        /* Il token nel QR resta valido finché non lo rigeneri manualmente
           (🔄 sopra): uno screenshot salvato/condiviso di questo QR è un
           segreto permanente, non un codice usa-e-getta — prima l'avviso
           copriva solo TLS/assenza token, non questo. */
        note += "<br><small style='color:#f59e0b;'>"
                "\xe2\x9a\xa0\xef\xb8\x8f" "  Il token resta valido finch\xc3\xa9 non lo rigeneri: "
                "non condividere screenshot di questo QR.</small>";
    }

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

void LanWanPage::onManualIpChanged()
{
    if (!m_lanIpOct[0]) return;
    const QString ip = QString("%1.%2.%3.%4")
        .arg(m_lanIpOct[0]->value())
        .arg(m_lanIpOct[1]->value())
        .arg(m_lanIpOct[2]->value())
        .arg(m_lanIpOct[3]->value());
    QSettings("Prismalux", "GUI").setValue("lan/manualIp", ip);
    onUpdateQrInline();
}

void LanWanPage::onManualMaskChanged()
{
    if (!m_lanMaskOct[0]) return;
    const QString mask = QString("%1.%2.%3.%4")
        .arg(m_lanMaskOct[0]->value())
        .arg(m_lanMaskOct[1]->value())
        .arg(m_lanMaskOct[2]->value())
        .arg(m_lanMaskOct[3]->value());
    QSettings("Prismalux", "GUI").setValue("lan/manualMask", mask);
}

void LanWanPage::onUpdateQrInline()
{
    if (!m_qrInlineWidget) return;
    const QString ip   = localLanIp();
    const int     port = m_lanPortSpin ? m_lanPortSpin->value() : 11500;
    QString token = m_lanTokenEdit ? m_lanTokenEdit->text().trimmed() : QString();
    if (token.isEmpty())
        token = LanServer::loadLanToken();   // fallback diretto al file
    QString url = QString("%1://%2:%3/web").arg(serverScheme()).arg(ip).arg(port);
    if (!token.isEmpty())
        url += "?token=" + QString::fromLatin1(QUrl::toPercentEncoding(token));

    m_qrInlineWidget->setText(url);
    m_qrInlineWidget->setToolTip(url);       // hover → URL con token visibile

    if (m_qrAndroidWidget) {
        m_qrAndroidWidget->setText(url);
        m_qrAndroidWidget->setToolTip(url);
    }

    if (m_urlDisplayLbl)
        m_urlDisplayLbl->setText(QString("%1 : %2").arg(ip).arg(port));
}

void LanWanPage::onIpWatchTick()
{
    const QString currentIp = localLanIp();
    if (currentIp == m_lastKnownIp) return;

    m_lastKnownIp = currentIp;
    onUpdateQrInline();  /* aggiorna QR con il nuovo IP */

    if (m_urlDisplayLbl)
        m_urlDisplayLbl->setText(
            QString("\xf0\x9f\x94\x84  %1 \xe2\x80\x94 IP cambiato, QR aggiornato")
                .arg(currentIp));
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
        const bool tlsOn = m_lanServer->isTlsEnabled();
        const QString proto = tlsOn
            ? "\xf0\x9f\x94\x92 HTTPS" : "\xf0\x9f\x94\x93 HTTP";
        if (!tlsOn && m_lanServer->isTlsRequested()) {
            /* TLS richiesto ma listen/openssl fallito → fallback HTTP silenzioso
             * in LanServer::start(): il token Bearer viaggia in chiaro sulla LAN */
            m_lanStatusLbl->setText(
                "\xe2\x97\x8f  Attivo — " + proto + " — porta " +
                QString::number(m_lanServer->port()) +
                " — \xe2\x9a\xa0\xef\xb8\x8f TLS non disponibile: token in chiaro");
            m_lanStatusLbl->setStyleSheet("color: #f59e0b; font-weight: bold;");
        } else {
            m_lanStatusLbl->setText(
                "\xe2\x97\x8f  Attivo — " + proto + " — porta " +
                QString::number(m_lanServer->port()));
            m_lanStatusLbl->setStyleSheet("color: #4caf50; font-weight: bold;");
        }
    } else {
        m_lanStatusLbl->setText(tr("\xe2\x97\x8b  Fermo"));
        m_lanStatusLbl->setStyleSheet("color: #9e9e9e;");
    }
    /* Rigenera QR/URL: alla costruzione della pagina il TLS non è ancora
     * attivo (serverScheme()="http"), diventa https solo dopo start() */
    onUpdateQrInline();
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
void LanWanPage::clientTableAddRow(const QString& ip, const QString& deviceName)
{
    if (!m_clientTable) return;
    const int row = m_clientTable->rowCount();
    m_clientTable->insertRow(row);
    m_clientTable->setItem(row, 0, new QTableWidgetItem(ip));
    m_clientTable->setItem(row, 1, new QTableWidgetItem(readMacForIp(ip)));
    /* Estrai nome leggibile dallo User-Agent (es. "Prismalux/3.0 (Android 14)") */
    QString devLabel = deviceName.trimmed();
    if (devLabel.isEmpty()) devLabel = tr("Sconosciuto");
    m_clientTable->setItem(row, 2, new QTableWidgetItem(devLabel));
    m_clientTable->setItem(row, 3, new QTableWidgetItem(
        QDateTime::currentDateTime().toString("dd/MM HH:mm:ss")));
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

void LanWanPage::onLanClientConnected(const QString& addr, const QString& deviceName)
{
    clientTableAddRow(addr, deviceName);
}

void LanWanPage::onLanClientDisconnected(const QString& addr)
{
    clientTableRemoveRow(addr);
}

/* ── Lista persone autorizzate ── */
void LanWanPage::loadAccessList()
{
    if (!m_accessListTable) return;
    const QJsonArray arr = QJsonDocument::fromJson(
        QSettings("Prismalux", "GUI").value("lan/accessList").toByteArray()
    ).array();
    m_accessListTable->setRowCount(0);
    for (const QJsonValue& v : arr) {
        const QJsonObject o = v.toObject();
        const int row = m_accessListTable->rowCount();
        m_accessListTable->insertRow(row);
        m_accessListTable->setItem(row, 0, new QTableWidgetItem(o["name"].toString()));
        m_accessListTable->setItem(row, 1, new QTableWidgetItem(o["added"].toString()));
    }
}

void LanWanPage::saveAccessList()
{
    if (!m_accessListTable) return;
    QJsonArray arr;
    for (int r = 0; r < m_accessListTable->rowCount(); ++r) {
        QJsonObject o;
        o["name"]  = m_accessListTable->item(r, 0) ? m_accessListTable->item(r, 0)->text() : QString();
        o["added"] = m_accessListTable->item(r, 1) ? m_accessListTable->item(r, 1)->text() : QString();
        arr.append(o);
    }
    QSettings("Prismalux", "GUI").setValue("lan/accessList",
        QJsonDocument(arr).toJson(QJsonDocument::Compact));
}

void LanWanPage::onAddPersonClicked()
{
    bool ok = false;
    const QString name = QInputDialog::getText(
        this, tr("Aggiungi persona autorizzata"),
        tr("Nome o descrizione (es. \"Mario - telefono\"):"),
        QLineEdit::Normal, {}, &ok);
    if (!ok || name.trimmed().isEmpty()) return;

    const int row = m_accessListTable->rowCount();
    m_accessListTable->insertRow(row);
    m_accessListTable->setItem(row, 0, new QTableWidgetItem(name.trimmed()));
    m_accessListTable->setItem(row, 1, new QTableWidgetItem(
        QDateTime::currentDateTime().toString("dd/MM/yyyy HH:mm")));
    saveAccessList();

    /* Copia il token negli appunti per facilitare la condivisione */
    const QString token = m_lanTokenEdit ? m_lanTokenEdit->text().trimmed()
                                         : LanServer::loadLanToken();
    if (!token.isEmpty()) {
        QApplication::clipboard()->setText(token);
        QToolTip::showText(QCursor::pos(),
            tr("Token copiato negli appunti!\nCondividilo con %1.").arg(name.trimmed()), this);
    }
}

void LanWanPage::onRemovePersonClicked()
{
    if (!m_accessListTable) return;
    const int row = m_accessListTable->currentRow();
    if (row < 0) return;
    m_accessListTable->removeRow(row);
    saveAccessList();
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
            m_lanToggleBtn->setText(tr("\xe2\x97\x8f  Server ON"));
            m_lanPortSpin->setEnabled(false);
            AppConfig::s().setValue(P::SK::kLanAutoStart, true);
            AppConfig::s().setValue(P::SK::kLanPort, (int)port);
        } else {
            m_lanToggleBtn->blockSignals(true);
            m_lanToggleBtn->setChecked(false);
            m_lanToggleBtn->blockSignals(false);
            m_lanStatusLbl->setText(tr("\xe2\x9d\x8c  Impossibile aprire la porta"));
            LogBus::post("\xe2\x9d\x8c LAN WAN: Impossibile aprire la porta LAN.");
            m_lanStatusLbl->setStyleSheet("color: #f44336;");
            AppConfig::s().setValue(P::SK::kLanAutoStart, false);
        }
    } else {
        if (m_lanServer) m_lanServer->stop();
        m_lanToggleBtn->setText(tr("\xe2\x97\x8b  Server OFF"));
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
    QString url = QString("%1://%2:%3/web")
                      .arg(serverScheme()).arg(localLanIp()).arg(m_lanServer->port());
    const QString token = m_lanTokenEdit ? m_lanTokenEdit->text().trimmed() : QString();
    if (!token.isEmpty())
        url += "?token=" + QString::fromLatin1(QUrl::toPercentEncoding(token));
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
    auto* srvGroup = new QGroupBox(tr("\xf0\x9f\x94\xa7  Controllo server"), leftW);
    auto* srvLay   = new QVBoxLayout(srvGroup);
    srvLay->setSpacing(6);

    auto* ctrlRow = new QHBoxLayout;
    m_lanToggleBtn = new QPushButton(tr("\xe2\x97\x8b  Server OFF"), srvGroup);
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
        auto* tokenLbl = new QLabel(tr("\xf0\x9f\x94\x91  Token:"), tokenRow);
        m_lanTokenEdit = new QLineEdit(tokenRow);
        m_lanTokenEdit->setPlaceholderText(tr("Auto-generato all\xe2\x80\x99" "avvio"));
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

    m_lanStatusLbl = new QLabel(tr("\xe2\x97\x8b  Fermo"), srvGroup);
    m_lanStatusLbl->setStyleSheet("color: #9e9e9e;");
    srvLay->addWidget(m_lanStatusLbl);

    /* Riga IP: 4 spinbox ottetti + bottone reset auto */
    {
        auto* ipRow = new QWidget(srvGroup);
        auto* ipLay = new QHBoxLayout(ipRow);
        ipLay->setContentsMargins(0, 0, 0, 0); ipLay->setSpacing(2);

        ipLay->addWidget(new QLabel(tr("IP:"), ipRow));

        /* Legge IP salvato; se non c'è usa l'auto-detect */
        QSettings sets("Prismalux", "GUI");
        const QString savedIp = sets.value("lan/manualIp", ip).toString();
        const QStringList parts = savedIp.split('.');

        for (int i = 0; i < 4; ++i) {
            if (i > 0) {
                auto* dot = new QLabel(".", ipRow);
                dot->setFixedWidth(dpiScale(6));
                dot->setAlignment(Qt::AlignCenter);
                ipLay->addWidget(dot);
            }
            m_lanIpOct[i] = new QSpinBox(ipRow);
            m_lanIpOct[i]->setRange(0, 255);
            m_lanIpOct[i]->setValue(parts.size() == 4 ? parts[i].toInt() : 0);
            m_lanIpOct[i]->setFixedWidth(dpiScale(72));
            m_lanIpOct[i]->setAlignment(Qt::AlignCenter);
            m_lanIpOct[i]->setToolTip(tr("Ottetto %1 dell'indirizzo IP (modifica o usa ▲▼)").arg(i + 1));
            connect(m_lanIpOct[i], QOverload<int>::of(&QSpinBox::valueChanged),
                    this, &LanWanPage::onManualIpChanged);
            ipLay->addWidget(m_lanIpOct[i]);
        }

        /* Bottone reset: torna all'IP auto-rilevato */
        auto* resetBtn = new QPushButton("\xe2\x86\xba", ipRow);
        resetBtn->setFixedWidth(dpiScale(28));
        resetBtn->setFlat(true);
        resetBtn->setToolTip(tr("Ripristina IP auto-rilevato"));
        connect(resetBtn, &QPushButton::clicked, this, [this]() {
            const QString detected = autoDetectIp();
            const QStringList p = detected.split('.');
            if (p.size() == 4)
                for (int i = 0; i < 4; ++i)
                    m_lanIpOct[i]->setValue(p[i].toInt());
        });

        ipLay->addSpacing(4);
        ipLay->addWidget(resetBtn);
        ipLay->addStretch();
        srvLay->addWidget(ipRow);
    }

    /* Riga Netmask: 4 spinbox ottetti */
    {
        auto* maskRow = new QWidget(srvGroup);
        auto* maskLay = new QHBoxLayout(maskRow);
        maskLay->setContentsMargins(0, 0, 0, 0); maskLay->setSpacing(2);

        maskLay->addWidget(new QLabel(tr("Mask:"), maskRow));

        QSettings sets("Prismalux", "GUI");
        const QString savedMask = sets.value("lan/manualMask", "255.255.255.0").toString();
        const QStringList mparts = savedMask.split('.');

        for (int i = 0; i < 4; ++i) {
            if (i > 0) {
                auto* dot = new QLabel(".", maskRow);
                dot->setFixedWidth(dpiScale(6));
                dot->setAlignment(Qt::AlignCenter);
                maskLay->addWidget(dot);
            }
            m_lanMaskOct[i] = new QSpinBox(maskRow);
            m_lanMaskOct[i]->setRange(0, 255);
            m_lanMaskOct[i]->setValue(mparts.size() == 4 ? mparts[i].toInt() : (i < 3 ? 255 : 0));
            m_lanMaskOct[i]->setFixedWidth(dpiScale(72));
            m_lanMaskOct[i]->setAlignment(Qt::AlignCenter);
            m_lanMaskOct[i]->setToolTip(tr("Ottetto %1 della netmask (modifica o usa ▲▼)").arg(i + 1));
            connect(m_lanMaskOct[i], QOverload<int>::of(&QSpinBox::valueChanged),
                    this, &LanWanPage::onManualMaskChanged);
            maskLay->addWidget(m_lanMaskOct[i]);
        }

        maskLay->addStretch();
        srvLay->addWidget(maskRow);
    }

    connect(m_lanPortSpin, QOverload<int>::of(&QSpinBox::valueChanged),
            this, &LanWanPage::onLanPortChanged);

    leftLay->addWidget(srvGroup);

    /* Tabella client connessi */
    auto* clientGroup = new QGroupBox(
        "\xf0\x9f\x94\x92  Controllo accessi al Server Locale", leftW);
    auto* clientLay   = new QVBoxLayout(clientGroup);
    clientLay->setSpacing(4);

    m_clientTable = new QTableWidget(0, 4, clientGroup);
    m_clientTable->setHorizontalHeaderLabels({tr("Indirizzo IP"), tr("MAC"), tr("Dispositivo"), tr("Connesso alle")});
    m_clientTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    m_clientTable->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    m_clientTable->horizontalHeader()->setSectionResizeMode(2, QHeaderView::Stretch);
    m_clientTable->horizontalHeader()->setSectionResizeMode(3, QHeaderView::ResizeToContents);
    m_clientTable->verticalHeader()->setVisible(false);
    m_clientTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_clientTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_clientTable->setAlternatingRowColors(true);
    clientLay->addWidget(m_clientTable, 1);

    auto* kickRow = new QWidget(clientGroup);
    auto* kickLay = new QHBoxLayout(kickRow);
    kickLay->setContentsMargins(0, 0, 0, 0); kickLay->setSpacing(6);
    m_kickBtn = new QPushButton(tr("\xf0\x9f\x9a\xab  Disconnetti selezionato"), kickRow);
    m_kickBtn->setObjectName("actionBtn");
    m_kickBtn->setProperty("danger", true);
    m_kickBtn->setToolTip(tr("Chiude la connessione del client selezionato"));
    m_kickAllBtn = new QPushButton(tr("\xf0\x9f\x9a\xab  Disconnetti tutti"), kickRow);
    m_kickAllBtn->setObjectName("actionBtn");
    m_kickAllBtn->setProperty("danger", true);
    m_kickAllBtn->setToolTip(tr("Chiude tutte le connessioni client attive"));
    kickLay->addWidget(m_kickBtn, 1);
    kickLay->addWidget(m_kickAllBtn, 1);
    clientLay->addWidget(kickRow);

    connect(m_kickBtn,    &QPushButton::clicked, this, &LanWanPage::onKickBtnClicked);
    connect(m_kickAllBtn, &QPushButton::clicked, this, &LanWanPage::onKickAllBtnClicked);

    leftLay->addWidget(clientGroup, 1);

    /* ── Persone autorizzate (rubrica chi ha il token) ── */
    auto* accessGroup = new QGroupBox(tr("\xf0\x9f\x91\xa5  Persone con accesso"), leftW);
    auto* accessLay   = new QVBoxLayout(accessGroup);
    accessLay->setSpacing(4);

    m_accessListTable = new QTableWidget(0, 2, accessGroup);
    m_accessListTable->setHorizontalHeaderLabels({tr("Nome / Dispositivo"), tr("Aggiunto il")});
    m_accessListTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    m_accessListTable->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    m_accessListTable->verticalHeader()->setVisible(false);
    m_accessListTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_accessListTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_accessListTable->setAlternatingRowColors(true);
    m_accessListTable->setMaximumHeight(dpiScale(130));
    accessLay->addWidget(m_accessListTable);

    auto* accessBtnRow = new QWidget(accessGroup);
    auto* accessBtnLay = new QHBoxLayout(accessBtnRow);
    accessBtnLay->setContentsMargins(0, 0, 0, 0); accessBtnLay->setSpacing(6);

    auto* addPersonBtn = new QPushButton(tr("\xe2\x9e\x95  Aggiungi persona"), accessBtnRow);
    addPersonBtn->setObjectName("primaryBtn");
    addPersonBtn->setToolTip(tr("Registra chi ha il token (copia token negli appunti)"));

    auto* removePersonBtn = new QPushButton(tr("\xe2\x9e\x96  Rimuovi"), accessBtnRow);
    removePersonBtn->setObjectName("actionBtn");
    removePersonBtn->setToolTip(tr("Rimuove la persona selezionata dalla lista"));

    accessBtnLay->addWidget(addPersonBtn, 1);
    accessBtnLay->addWidget(removePersonBtn);
    accessLay->addWidget(accessBtnRow);

    connect(addPersonBtn,    &QPushButton::clicked, this, &LanWanPage::onAddPersonClicked);
    connect(removePersonBtn, &QPushButton::clicked, this, &LanWanPage::onRemovePersonClicked);

    loadAccessList();
    leftLay->addWidget(accessGroup);

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

    /* ── Coppia QR affiancata: sinistra = web generico, destra = app Android ── */
    auto* qrPairW    = new QWidget(rightW);
    auto* qrPairLay  = new QHBoxLayout(qrPairW);
    qrPairLay->setContentsMargins(0, 0, 0, 0);
    qrPairLay->setSpacing(dpiScale(10));

    /* — colonna sinistra: QR generico (browser web) — */
    auto* qrWebCol    = new QWidget(qrPairW);
    auto* qrWebColLay = new QVBoxLayout(qrWebCol);
    qrWebColLay->setContentsMargins(0, 0, 0, 0);
    qrWebColLay->setSpacing(4);

    auto* qrWebLbl = new QLabel(tr("\xf0\x9f\x8c\x90  <b>Web Chat</b>"), qrWebCol);
    qrWebLbl->setTextFormat(Qt::RichText);
    qrWebLbl->setAlignment(Qt::AlignCenter);
    qrWebLbl->setStyleSheet("font-size:12px;");
    qrWebColLay->addWidget(qrWebLbl, 0, Qt::AlignHCenter);

    m_qrInlineWidget = new QrCodeWidget(QString(), qrWebCol);
    m_qrInlineWidget->setFixedSize(dpiSize(130, 130));
    m_qrInlineWidget->setToolTip(
        "QR web: apre la chat nel browser del telefono.\n"
        "Si aggiorna automaticamente con IP, porta e token.");
    qrWebColLay->addWidget(m_qrInlineWidget, 0, Qt::AlignHCenter);

    auto* qrWebHintLbl = new QLabel(tr("<small>Apri nel browser</small>"), qrWebCol);
    qrWebHintLbl->setTextFormat(Qt::RichText);
    qrWebHintLbl->setAlignment(Qt::AlignCenter);
    qrWebHintLbl->setStyleSheet("color: gray; font-size:11px;");
    qrWebColLay->addWidget(qrWebHintLbl, 0, Qt::AlignHCenter);

    /* — colonna destra: QR app Flutter Android — */
    auto* qrAppCol    = new QWidget(qrPairW);
    auto* qrAppColLay = new QVBoxLayout(qrAppCol);
    qrAppColLay->setContentsMargins(0, 0, 0, 0);
    qrAppColLay->setSpacing(4);

    auto* qrAppLbl = new QLabel(tr("\xf0\x9f\x93\xb1  <b>App Android</b>"), qrAppCol);
    qrAppLbl->setTextFormat(Qt::RichText);
    qrAppLbl->setAlignment(Qt::AlignCenter);
    qrAppLbl->setStyleSheet("font-size:12px;");
    qrAppColLay->addWidget(qrAppLbl, 0, Qt::AlignHCenter);

    m_qrAndroidWidget = new QrCodeWidget(QString(), qrAppCol);
    m_qrAndroidWidget->setFixedSize(dpiSize(130, 130));
    m_qrAndroidWidget->setToolTip(
        "QR app Flutter: scansiona con PrismaluxMobile\n"
        "Impostazioni \xe2\x86\x92 Scansiona QR\n"
        "Configura IP, porta e token in automatico.");
    qrAppColLay->addWidget(m_qrAndroidWidget, 0, Qt::AlignHCenter);

    auto* qrAppHintLbl = new QLabel(
        "<small>Impostazioni \xe2\x86\x92 \xf0\x9f\x93\xb7 Scansiona QR</small>",
        qrAppCol);
    qrAppHintLbl->setTextFormat(Qt::RichText);
    qrAppHintLbl->setAlignment(Qt::AlignCenter);
    qrAppHintLbl->setStyleSheet("color: gray; font-size:11px;");
    qrAppColLay->addWidget(qrAppHintLbl, 0, Qt::AlignHCenter);

    qrPairLay->addWidget(qrWebCol, 1);
    qrPairLay->addWidget(qrAppCol, 1);
    rightLay->addWidget(qrPairW, 0, Qt::AlignTop);

    /* ── Istruzioni rapide ── */
    auto* qrInfoLbl = new QLabel(rightW);
    qrInfoLbl->setTextFormat(Qt::RichText);
    qrInfoLbl->setWordWrap(true);
    qrInfoLbl->setAlignment(Qt::AlignLeft | Qt::AlignTop);
    qrInfoLbl->setText(
        "<br>"
        "<span style='font-size:13px;'>"
        "<b>\xf0\x9f\x93\xb1  Connetti l\xe2\x80\x99" "app Android</b></span><br>"
        "<span style='font-size:12px;'>"
        "1. Avvia <b>PrismaluxMobile</b> sul telefono<br>"
        "2. Apri <b>Impostazioni</b> \xe2\x86\x92 "
        "<b>\xf0\x9f\x93\xb7 Scansiona QR</b><br>"
        "3. Punta la fotocamera sul QR <b>App Android</b> (destra)<br>"
        "<span style='color:gray;font-size:11px;'>"
        "IP + Porta + Token configurati in automatico.</span></span>");
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
    urlCopyBtn->setToolTip(tr("Copia URL negli appunti"));
    urlCopyBtn->setObjectName("actionBtn");
    urlCopyBtn->setAccessibleName(tr("Copia URL server LAN negli appunti"));

    urlRowL->addWidget(urlIcon);
    urlRowL->addWidget(m_urlDisplayLbl, 1);
    urlRowL->addWidget(urlCopyBtn);
    scrollLay->addWidget(urlRow);

    connect(urlCopyBtn, &QPushButton::clicked, urlCopyBtn, [this, urlCopyBtn]{
        const QString ip   = localLanIp();
        const int     port = m_lanPortSpin ? m_lanPortSpin->value() : 11500;
        QString token = m_lanTokenEdit ? m_lanTokenEdit->text().trimmed() : QString();
        if (token.isEmpty()) token = LanServer::loadLanToken();
        QString url = QString("%1://%2:%3/web").arg(serverScheme()).arg(ip).arg(port);
        if (!token.isEmpty())
            url += "?token=" + QString::fromLatin1(QUrl::toPercentEncoding(token));
        QGuiApplication::clipboard()->setText(url);
        urlCopyBtn->setText(tr("\xe2\x9c\x85"));                  /* ✅ feedback */
        QTimer::singleShot(1500, urlCopyBtn, [urlCopyBtn]{
            urlCopyBtn->setText(tr("\xf0\x9f\x93\x8b"));
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
    qrConnectBtn->setToolTip(tr("Mostra il QR in un dialogo grande"));
    connect(qrConnectBtn, &QPushButton::clicked,
            this, &LanWanPage::onQrConnectBtnClicked);
    scrollLay->addWidget(qrConnectBtn);

    /* QR APK + Pagina */
    auto* qrRow  = new QWidget(scrollW);
    auto* qrRowL = new QHBoxLayout(qrRow);
    qrRowL->setContentsMargins(0, 0, 0, 0); qrRowL->setSpacing(6);
    m_qrApkBtn  = new QPushButton(tr("\xf0\x9f\x93\xa6  QR APK"),  qrRow);
    m_qrApkBtn->setObjectName("actionBtn");
    m_qrApkBtn->setEnabled(false);
    m_qrPageBtn = new QPushButton(tr("\xf0\x9f\x8c\x90  QR Pagina"), qrRow);
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
    m_adbInstallBtn->setAccessibleName(tr("Installa APK PrismaluxMobile sul telefono Android via USB"));
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
    m_adbLog->setPlaceholderText(tr("Output adb..."));
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

    /* Polling IP LAN ogni 30s — aggiorna QR automaticamente se l'IP cambia (DHCP) */
    m_lastKnownIp = localLanIp();
    m_ipWatchTimer = new QTimer(this);
    m_ipWatchTimer->setInterval(30'000);
    m_ipWatchTimer->setSingleShot(false);
    connect(m_ipWatchTimer, &QTimer::timeout, this, &LanWanPage::onIpWatchTick);
    m_ipWatchTimer->start();

    return tab;
}
