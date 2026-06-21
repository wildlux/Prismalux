#include "main_maintenance.h"
#include "../dpi_utils.h"
#include "../lan_server.h"
#include "../widgets/qr_code_widget.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGroupBox>
#include <QSpinBox>
#include <QDialog>
#include <QLabel>
#include <QPushButton>
#include <QClipboard>
#include <QApplication>
#include <QNetworkInterface>
#include <QHostAddress>
#include <QSettings>
#include <QStandardPaths>
#include <QDir>
#include <QTextEdit>
#include <QTimer>
#include <QUuid>

/* ── IP locale LAN — preferisce 192.168.x.x su 10.x.x.x ────────────────── */
static QString localLanIP()
{
    QString fallback;
    for (const QNetworkInterface& iface : QNetworkInterface::allInterfaces()) {
        if (iface.flags().testFlag(QNetworkInterface::IsLoopBack)) continue;
        if (!iface.flags().testFlag(QNetworkInterface::IsUp))      continue;
        for (const QNetworkAddressEntry& entry : iface.addressEntries()) {
            if (entry.ip().protocol() != QAbstractSocket::IPv4Protocol) continue;
            const QString s = entry.ip().toString();
            if (s.startsWith("192.168.")) return s;
            if ((s.startsWith("10.") || s.startsWith("172.")) && fallback.isEmpty())
                fallback = s;
        }
    }
    return fallback.isEmpty() ? "127.0.0.1" : fallback;
}

/* ════════════════════════════════════════════════════════════════════════════
   ManutenzioneePage::buildLanServer
   ════════════════════════════════════════════════════════════════════════════ */
QWidget* ManutenzioneePage::buildLanServer()
{
    auto* w    = new QWidget;
    auto* vbox = new QVBoxLayout(w);
    vbox->setContentsMargins(12, 12, 12, 12);
    vbox->setSpacing(10);

    /* ── Titolo sezione ── */
    auto* titleLbl = new QLabel(
        "<b>" "\xf0\x9f\x8c\x90" " Server LAN per Android</b>", w);  // 🌐
    titleLbl->setTextFormat(Qt::RichText);
    vbox->addWidget(titleLbl);

    /* ── GroupBox principale ── */
    auto* group = new QGroupBox(w);
    group->setObjectName("LanServerGroup");
    auto* gl = new QVBoxLayout(group);
    gl->setSpacing(8);

    /* Riga: ON/OFF + porta */
    auto* ctrlRow = new QHBoxLayout;
    m_lanToggleBtn = new QPushButton("\xe2\x97\x8b" "  Server OFF", group);  // ○
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

    /* Stato */
    m_lanStatusLbl = new QLabel("\xe2\x97\x8b" "  Fermo", group);  // ○
    m_lanStatusLbl->setStyleSheet("color: #9e9e9e;");
    gl->addWidget(m_lanStatusLbl);

    /* Client connessi */
    m_lanClientsLbl = new QLabel("Client connessi: 0", group);
    gl->addWidget(m_lanClientsLbl);

    /* IP locale */
    const QString ip = localLanIP();
    auto* ipLbl = new QLabel(
        QString("IP del PC: <b>%1</b>").arg(ip), group);
    ipLbl->setTextFormat(Qt::RichText);
    gl->addWidget(ipLbl);

    /* Avviso selezione modello */
    auto* noteLbl = new QLabel(
        "<small><i>I modelli cloud si selezionano dalla scheda "
        "Strumenti AI \xe2\x86\x92 modello</i></small>",  // →
        group);
    noteLbl->setTextFormat(Qt::RichText);
    noteLbl->setWordWrap(true);
    gl->addWidget(noteLbl);

    /* ── Bottone QR Code APK ── */
    auto* qrBtn = new QPushButton(
        "\xf0\x9f\x93\xb1" "  Scarica APK (QR Code)", group);  // 📱
    qrBtn->setObjectName("actionBtn");
    qrBtn->setToolTip(tr("Mostra il QR code per scaricare PrismaluxMobile.apk sul telefono"));
    qrBtn->setEnabled(false);
    gl->addWidget(qrBtn);

    vbox->addWidget(group);

    /* ── Sezione TLS ── */
    auto* tlsGroup = new QGroupBox(w);
    tlsGroup->setObjectName("TlsGroup");
    auto* tlsLay = new QVBoxLayout(tlsGroup);
    tlsLay->setSpacing(6);

    auto* tlsTitleLbl = new QLabel(
        "<b>" "\xf0\x9f\x94\x92" " TLS — cifratura connessione LAN</b>", tlsGroup);
    tlsTitleLbl->setTextFormat(Qt::RichText);
    tlsLay->addWidget(tlsTitleLbl);

    auto* tlsCtrlRow = new QWidget(tlsGroup);
    auto* tlsCtrlLay = new QHBoxLayout(tlsCtrlRow);
    tlsCtrlLay->setContentsMargins(0,0,0,0); tlsCtrlLay->setSpacing(8);

    m_tlsGenBtn = new QPushButton(
        "\xf0\x9f\x94\x92  Genera certificato TLS self-signed", tlsGroup);
    m_tlsGenBtn->setObjectName("actionBtn");
    m_tlsGenBtn->setToolTip(
        "Genera ~/.prismalux/lan_cert.pem e ~/.prismalux/lan_key.pem\n"
        "tramite openssl (RSA 2048, 365 giorni).\n"
        "Richiede openssl installato sul sistema.");

    m_tlsEnableCheck = new QCheckBox(
        "\xf0\x9f\x94\x92  Abilita TLS (richiede certificato)", tlsGroup);
    {
        QSettings s("Prismalux", "GUI");
        m_tlsEnableCheck->setChecked(s.value("lan/tls_enabled", false).toBool());
    }
    m_tlsEnableCheck->setToolTip(
        "Se attivo, il server LAN usa HTTPS/WSS invece di HTTP.\n"
        "Genera prima il certificato con il pulsante a sinistra.");

    tlsCtrlLay->addWidget(m_tlsGenBtn);
    tlsCtrlLay->addWidget(m_tlsEnableCheck);
    tlsCtrlLay->addStretch(1);
    tlsLay->addWidget(tlsCtrlRow);

    m_tlsLog = new QTextEdit(tlsGroup);
    m_tlsLog->setReadOnly(true);
    m_tlsLog->setFixedHeight(dpiScale(72));
    m_tlsLog->setPlaceholderText(tr("Output generazione certificato TLS\xe2\x80\xa6"));
    tlsLay->addWidget(m_tlsLog);

    vbox->addWidget(tlsGroup);

    /* ── Sezione Token di accesso LAN ── */
    auto* tokenGroup = new QGroupBox(w);
    auto* tokenLay   = new QVBoxLayout(tokenGroup);
    tokenLay->setSpacing(6);

    auto* tokenTitleLbl = new QLabel(
        "<b>" "\xf0\x9f\x94\x91" " Token di accesso LAN</b>", tokenGroup);
    tokenTitleLbl->setTextFormat(Qt::RichText);
    tokenLay->addWidget(tokenTitleLbl);

    auto* tokenInfoLbl = new QLabel(
        "<small>Il token autentica le richieste API dalle app Android. "
        "Rigenerarlo invalida le sessioni precedenti.</small>", tokenGroup);
    tokenInfoLbl->setTextFormat(Qt::RichText);
    tokenInfoLbl->setWordWrap(true);
    tokenLay->addWidget(tokenInfoLbl);

    {
        const QString tok = LanServer::loadLanToken();
        const QString masked = tok.isEmpty()
            ? tr("(auto-generato all\xe2\x80\x99avvio server)")
            : tok.left(4) + "\xe2\x80\xa6" + tok.right(4);
        m_tokenLbl = new QLabel("<code>" + masked + "</code>", tokenGroup);
        m_tokenLbl->setTextFormat(Qt::RichText);
        tokenLay->addWidget(m_tokenLbl);
    }

    m_regenBtn = new QPushButton(
        "\xf0\x9f\x94\x84" "  Rigenera Token", tokenGroup);
    m_regenBtn->setObjectName("actionBtn");
    m_regenBtn->setToolTip(tr(
        "Genera un nuovo token UUID e lo copia negli appunti.\n"
        "Il vecchio token viene invalidato immediatamente se il server \xc3\xa8 attivo."));
    connect(m_regenBtn, &QPushButton::clicked,
            this, &ManutenzioneePage::onRegenTokenClicked);
    tokenLay->addWidget(m_regenBtn);

    vbox->addWidget(tokenGroup);
    vbox->addStretch();

    m_qrBtn = qrBtn;

    /* ── Abilita QR btn quando server è attivo ── */
    connect(m_lanToggleBtn, &QPushButton::toggled, this, &ManutenzioneePage::onLanQrBtnEnableUpdate);

    /* ── Apri dialog QR Code APK ── */
    connect(m_qrBtn, &QPushButton::clicked, this, &ManutenzioneePage::onQrBtnClicked);

    /* ── Toggle ON/OFF ── */
    connect(m_lanToggleBtn, &QPushButton::toggled, this, &ManutenzioneePage::onLanToggleBtnToggled);

    /* ── TLS ── */
    connect(m_tlsGenBtn, &QPushButton::clicked, this, &ManutenzioneePage::onTlsGenBtnClicked);
    connect(m_tlsEnableCheck, &QCheckBox::toggled, this, &ManutenzioneePage::onTlsEnableToggled);

    return w;
}

/* ──────────────────────────────────────────────────────────────
   Slot: abilita/disabilita pulsante QR in base allo stato server
   ────────────────────────────────────────────────────────────── */
void ManutenzioneePage::onLanQrBtnEnableUpdate(bool on)
{
    if (m_qrBtn)
        m_qrBtn->setEnabled(on && m_lanServer && m_lanServer->isRunning());
}

/* ──────────────────────────────────────────────────────────────
   Slot: apre il dialog QR Code per il download APK
   ────────────────────────────────────────────────────────────── */
void ManutenzioneePage::onQrBtnClicked()
{
    if (!m_lanServer || !m_lanServer->isRunning()) return;
    m_apkUrl = QString("http://%1:%2/apk")
                   .arg(localLanIP())
                   .arg(m_lanServer->port());

    auto* dlg = new QDialog(window());
    dlg->setWindowTitle(tr("Scarica PrismaluxMobile"));
    dlg->setAttribute(Qt::WA_DeleteOnClose);
    auto* vl = new QVBoxLayout(dlg);
    vl->setSpacing(12);
    vl->setContentsMargins(20, 20, 20, 20);

    auto* titleLbl = new QLabel(
        "<b>" "\xf0\x9f\x93\xb1" " Scansiona con il telefono per scaricare l'APK</b>",
        dlg);
    titleLbl->setTextFormat(Qt::RichText);
    titleLbl->setAlignment(Qt::AlignCenter);
    vl->addWidget(titleLbl);

    auto* qrw = new QrCodeWidget(m_apkUrl, dlg);
    qrw->setFixedSize(280, 280);
    vl->addWidget(qrw, 0, Qt::AlignCenter);

    auto* urlLbl = new QLabel(QString("<code>%1</code>").arg(m_apkUrl), dlg);
    urlLbl->setTextFormat(Qt::RichText);
    urlLbl->setAlignment(Qt::AlignCenter);
    urlLbl->setTextInteractionFlags(Qt::TextSelectableByMouse);
    vl->addWidget(urlLbl);

    auto* copyBtn = new QPushButton("\xf0\x9f\x93\x8b" "  Copia URL", dlg);  // 📋
    connect(copyBtn, &QPushButton::clicked, this, &ManutenzioneePage::onCopyApkUrlClicked);
    vl->addWidget(copyBtn);

    m_copyBtn = copyBtn;

    auto* noteApk = new QLabel(
        "<small><i>Il server LAN deve rimanere attivo durante il download.<br>"
        "Su Android: consenti installazione da sorgenti sconosciute.</i></small>",
        dlg);
    noteApk->setTextFormat(Qt::RichText);
    noteApk->setAlignment(Qt::AlignCenter);
    noteApk->setWordWrap(true);
    vl->addWidget(noteApk);

    dlg->resize(340, 480);
    dlg->exec();
}

/* ──────────────────────────────────────────────────────────────
   Slot: copia l'URL APK negli appunti
   ────────────────────────────────────────────────────────────── */
void ManutenzioneePage::onCopyApkUrlClicked()
{
    QApplication::clipboard()->setText(m_apkUrl);
    if (m_copyBtn) m_copyBtn->setText("\xe2\x9c\x85" "  Copiato!");  // ✅
}

/* ──────────────────────────────────────────────────────────────
   Slot: toggle server LAN ON/OFF
   ────────────────────────────────────────────────────────────── */
void ManutenzioneePage::onLanToggleBtnToggled(bool on)
{
    if (on) {
        if (!m_lanServer) {
            m_lanServer = new LanServer(m_ai, this);

            connect(m_lanServer, &LanServer::statusChanged,
                    this, &ManutenzioneePage::onLanServerStatusChanged);
            connect(m_lanServer, &LanServer::clientConnected,
                    this, &ManutenzioneePage::onLanClientConnected);
            connect(m_lanServer, &LanServer::clientDisconnected,
                    this, &ManutenzioneePage::onLanClientDisconnected);
        }

        /* Applica impostazione TLS salvata */
        {
            QSettings s("Prismalux", "GUI");
            m_lanServer->setTlsEnabled(s.value("lan/tls_enabled", false).toBool());
        }
        const quint16 port = static_cast<quint16>(m_lanPortSpin->value());
        if (m_lanServer->start(port)) {
            m_lanToggleBtn->setText("\xe2\x97\x8f" "  Server ON");  // ●
            m_lanPortSpin->setEnabled(false);
        } else {
            m_lanToggleBtn->blockSignals(true);
            m_lanToggleBtn->setChecked(false);
            m_lanToggleBtn->blockSignals(false);
            m_lanStatusLbl->setText(tr("\xe2\x9d\x8c  Impossibile aprire la porta"));  // ❌
            m_lanStatusLbl->setStyleSheet("color: #f44336;");
        }
    } else {
        if (m_lanServer) m_lanServer->stop();
        m_lanToggleBtn->setText("\xe2\x97\x8b" "  Server OFF");  // ○
        m_lanPortSpin->setEnabled(true);
    }
}

/* ──────────────────────────────────────────────────────────────
   Slot: stato server LAN cambiato
   ────────────────────────────────────────────────────────────── */
void ManutenzioneePage::onLanServerStatusChanged(bool running)
{
    const QString ip = localLanIP();
    if (running) {
        m_lanStatusLbl->setText(
            "\xe2\x97\x8f" "  Attivo su " + ip +
            ":" + QString::number(m_lanServer->port()));  // ●
        m_lanStatusLbl->setStyleSheet("color: #4caf50; font-weight: bold;");
    } else {
        m_lanStatusLbl->setText("\xe2\x97\x8b" "  Fermo");  // ○
        m_lanStatusLbl->setStyleSheet("color: #9e9e9e;");
        m_lanClientsLbl->setText(tr("Client connessi: 0"));
    }
}

/* ──────────────────────────────────────────────────────────────
   Slot: client connesso/disconnesso
   ────────────────────────────────────────────────────────────── */
void ManutenzioneePage::onLanClientConnected(const QString&)
{
    m_lanClientsLbl->setText(
        "Client connessi: " + QString::number(m_lanServer->clientCount()));
}

void ManutenzioneePage::onLanClientDisconnected(const QString&)
{
    m_lanClientsLbl->setText(
        "Client connessi: " + QString::number(m_lanServer->clientCount()));
}

void ManutenzioneePage::onTlsGenBtnClicked()
{
    if (m_tlsProc && m_tlsProc->state() != QProcess::NotRunning) return;

    const QString prismaDir =
        QStandardPaths::writableLocation(QStandardPaths::HomeLocation) +
        "/.prismalux";
    QDir().mkpath(prismaDir);

    const QString keyPath  = prismaDir + "/lan_key.pem";
    const QString certPath = prismaDir + "/lan_cert.pem";

    if (m_tlsLog) m_tlsLog->clear();

    m_tlsProc = new QProcess(this);
    connect(m_tlsProc, &QProcess::readyReadStandardOutput,
            this, &ManutenzioneePage::onTlsProcReadyRead);
    connect(m_tlsProc, &QProcess::readyReadStandardError,
            this, &ManutenzioneePage::onTlsProcReadyRead);
    connect(m_tlsProc, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, &ManutenzioneePage::onTlsProcFinished);
    connect(m_tlsProc, &QProcess::errorOccurred, this,
        [this](QProcess::ProcessError err) {
            if (err == QProcess::FailedToStart && m_tlsLog) {
                m_tlsLog->append(tr("\xe2\x9d\x8c  openssl non trovato \xe2\x80\x94 sudo apt install openssl"));
                if (m_tlsGenBtn) m_tlsGenBtn->setEnabled(true);
            }
        });

    const QStringList args{
        "req", "-x509", "-newkey", "rsa:2048",
        "-keyout", keyPath,
        "-out",    certPath,
        "-days",   "365",
        "-nodes",
        "-subj",   "/CN=prismalux-local"
    };
    m_tlsProc->start("openssl", args);
    if (m_tlsLog) m_tlsLog->append("Generazione certificato TLS in corso\xe2\x80\xa6");
    if (m_tlsGenBtn) m_tlsGenBtn->setEnabled(false);
}

void ManutenzioneePage::onTlsProcReadyRead()
{
    if (!m_tlsProc || !m_tlsLog) return;
    const QString out = m_tlsProc->readAllStandardOutput() +
                        m_tlsProc->readAllStandardError();
    if (!out.trimmed().isEmpty())
        m_tlsLog->append(out.trimmed());
}

void ManutenzioneePage::onTlsProcFinished(int code, QProcess::ExitStatus)
{
    if (m_tlsGenBtn) m_tlsGenBtn->setEnabled(true);
    if (!m_tlsLog) return;
    if (code == 0) {
        m_tlsLog->append(
            "\xe2\x9c\x85  Certificato generato in ~/.prismalux/");
    } else {
        m_tlsLog->append(
            "\xe2\x9d\x8c  Errore generazione certificato (codice " +
            QString::number(code) + "). Verificare che openssl sia installato.");
    }
    m_tlsProc->deleteLater();
    m_tlsProc = nullptr;
}

void ManutenzioneePage::onTlsEnableToggled(bool on)
{
    QSettings s("Prismalux", "GUI");
    s.setValue("lan/tls_enabled", on);
    if (m_lanServer) m_lanServer->setTlsEnabled(on);
}

void ManutenzioneePage::onRegenTokenClicked()
{
    const QString newToken =
        QUuid::createUuid().toString(QUuid::WithoutBraces).remove('-');
    LanServer::saveLanToken(newToken);
    if (m_lanServer)
        m_lanServer->setAccessToken(newToken);

    if (m_tokenLbl) {
        const QString masked = newToken.left(4) + "\xe2\x80\xa6" + newToken.right(4);
        m_tokenLbl->setText("<code>" + masked + "</code>");
    }
    QApplication::clipboard()->setText(newToken);

    if (m_regenBtn) {
        m_regenBtn->setText("\xe2\x9c\x85  Rigenerato (copiato negli appunti)");
        QTimer::singleShot(2500, m_regenBtn,
            [this]{ if (m_regenBtn) m_regenBtn->setText("\xf0\x9f\x94\x84  Rigenera Token"); });
    }
}
