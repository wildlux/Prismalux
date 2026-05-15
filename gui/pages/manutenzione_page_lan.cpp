#include "manutenzione_page.h"
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
    qrBtn->setToolTip("Mostra il QR code per scaricare PrismaluxMobile.apk sul telefono");
    qrBtn->setEnabled(false);
    gl->addWidget(qrBtn);

    vbox->addWidget(group);
    vbox->addStretch();

    /* ── Abilita QR btn quando server è attivo ── */
    connect(m_lanToggleBtn, &QPushButton::toggled, this, [this, qrBtn, ip](bool on) {
        qrBtn->setEnabled(on && m_lanServer && m_lanServer->isRunning());
        (void)ip;
    });

    /* ── Apri dialog QR Code APK ── */
    connect(qrBtn, &QPushButton::clicked, this, [this, qrBtn]() {
        if (!m_lanServer || !m_lanServer->isRunning()) return;
        const QString apkUrl = QString("http://%1:%2/apk")
                                   .arg(localLanIP())
                                   .arg(m_lanServer->port());

        auto* dlg = new QDialog(qrBtn->window());
        dlg->setWindowTitle("Scarica PrismaluxMobile");
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

        auto* qrw = new QrCodeWidget(apkUrl, dlg);
        qrw->setFixedSize(280, 280);
        vl->addWidget(qrw, 0, Qt::AlignCenter);

        auto* urlLbl = new QLabel(QString("<code>%1</code>").arg(apkUrl), dlg);
        urlLbl->setTextFormat(Qt::RichText);
        urlLbl->setAlignment(Qt::AlignCenter);
        urlLbl->setTextInteractionFlags(Qt::TextSelectableByMouse);
        vl->addWidget(urlLbl);

        auto* copyBtn = new QPushButton("\xf0\x9f\x93\x8b" "  Copia URL", dlg);  // 📋
        connect(copyBtn, &QPushButton::clicked, dlg, [apkUrl, copyBtn]() {
            QApplication::clipboard()->setText(apkUrl);
            copyBtn->setText("\xe2\x9c\x85" "  Copiato!");  // ✅
        });
        vl->addWidget(copyBtn);

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
    });

    /* ── Toggle ON/OFF ── */
    connect(m_lanToggleBtn, &QPushButton::toggled, this, [this](bool on) {
        if (on) {
            /* Lazy init + connessione segnali (una sola volta) */
            if (!m_lanServer) {
                m_lanServer = new LanServer(m_ai, this);

                connect(m_lanServer, &LanServer::statusChanged, this, [this](bool running) {
                    const QString ip2 = localLanIP();
                    if (running) {
                        m_lanStatusLbl->setText(
                            "\xe2\x97\x8f" "  Attivo su " + ip2 +
                            ":" + QString::number(m_lanServer->port()));  // ●
                        m_lanStatusLbl->setStyleSheet("color: #4caf50; font-weight: bold;");
                    } else {
                        m_lanStatusLbl->setText("\xe2\x97\x8b" "  Fermo");  // ○
                        m_lanStatusLbl->setStyleSheet("color: #9e9e9e;");
                        m_lanClientsLbl->setText("Client connessi: 0");
                    }
                });

                connect(m_lanServer, &LanServer::clientConnected,
                        this, [this](const QString&) {
                    m_lanClientsLbl->setText(
                        "Client connessi: " + QString::number(m_lanServer->clientCount()));
                });

                connect(m_lanServer, &LanServer::clientDisconnected,
                        this, [this](const QString&) {
                    m_lanClientsLbl->setText(
                        "Client connessi: " + QString::number(m_lanServer->clientCount()));
                });
            }

            const quint16 port = static_cast<quint16>(m_lanPortSpin->value());
            if (m_lanServer->start(port)) {
                m_lanToggleBtn->setText("\xe2\x97\x8f" "  Server ON");  // ●
                m_lanPortSpin->setEnabled(false);
            } else {
                m_lanToggleBtn->blockSignals(true);
                m_lanToggleBtn->setChecked(false);
                m_lanToggleBtn->blockSignals(false);
                m_lanStatusLbl->setText(
                    "\xe2\x9d\x8c  Impossibile aprire la porta");  // ❌
                m_lanStatusLbl->setStyleSheet("color: #f44336;");
            }
        } else {
            if (m_lanServer) m_lanServer->stop();
            m_lanToggleBtn->setText("\xe2\x97\x8b" "  Server OFF");  // ○
            m_lanPortSpin->setEnabled(true);
        }
    });

    return w;
}
