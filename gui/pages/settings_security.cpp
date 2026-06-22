/* ══════════════════════════════════════════════════════════════
   settings_security.cpp — Scheda "🔒 Sicurezza WAN & VPN"
   Impostazioni → tab Sicurezza

   Sezioni:
     1. Certificato TLS WAN — rigenera cert, mostra fingerprint,
        esporta pin per worker, importa pin server
     2. Chiavi WireGuard — genera coppia di chiavi, mostra
        pubkey, genera template wg0.conf
   ══════════════════════════════════════════════════════════════ */
#include "settings_main.h"
#include "../dpi_utils.h"
#include "../prismalux_paths.h"
#include "../widgets/proc_helper.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGroupBox>
#include <QLabel>
#include <QPushButton>
#include <QLineEdit>
#include <QTextEdit>
#include <QFileDialog>
#include <QClipboard>
#include <QApplication>
#include <QMessageBox>
#include <QSaveFile>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QCryptographicHash>
#include <QSslCertificate>
#include <QProcess>
#include <QScrollArea>
#include <QFrame>
#include <QTextBrowser>
#include <QSplitter>

namespace P = PrismaluxPaths;

/* ── Helpers interni ──────────────────────────────────────────── */
static QString certPath()   { return QDir::homePath() + "/.prismalux/server.crt"; }
static QString keyPath()    { return QDir::homePath() + "/.prismalux/server.key"; }
static QString pinSrvPath() { return QDir::homePath() + "/.prismalux/wan_cert.pin"; }
static QString pinCliPath() { return QDir::homePath() + "/.prismalux/wan_server.pin"; }

static QString readPin(const QString& path) {
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) return {};
    return QString::fromLatin1(f.readAll()).trimmed();
}

static bool regenCert() {
    QDir().mkpath(QDir::homePath() + "/.prismalux");
    QFile::remove(certPath());
    QFile::remove(keyPath());
    const auto r = ProcHelper::run("openssl", {
        "req", "-x509", "-newkey", "rsa:2048", "-nodes",
        "-days", "3650",
        "-keyout", keyPath(), "-out", certPath(),
        "-subj", "/CN=Prismalux-WAN"
    }, 15000);
    if (!r.ok || !QFileInfo::exists(certPath())) return false;
    QFile::setPermissions(keyPath(),
        QFileDevice::ReadOwner | QFileDevice::WriteOwner);
    /* Ricalcola e salva il pin */
    QFile cf(certPath());
    if (!cf.open(QIODevice::ReadOnly)) return false;
    const QSslCertificate cert(&cf, QSsl::Pem);
    if (cert.isNull()) return false;
    const QByteArray fp = cert.digest(QCryptographicHash::Sha256).toHex();
    QSaveFile pf(pinSrvPath());
    if (pf.open(QIODevice::WriteOnly | QIODevice::Text)) {
        pf.write(fp + "\n");
        pf.commit();
        QFile::setPermissions(pinSrvPath(),
            QFileDevice::ReadOwner | QFileDevice::WriteOwner);
    }
    return true;
}

/* ── Azioni TLS: rigenera, esporta, importa ──────────────────── */
static void secDoRegenCert(QPushButton* btn, QLineEdit* fpEdit) {
    const auto r = QMessageBox::question(btn->window(),
        "Rigenera certificato",
        "Verrà generato un nuovo certificato TLS.\n"
        "Tutti i worker dovranno aggiornare il loro pin.\n\nProcedere?");
    if (r != QMessageBox::Yes) return;
    btn->setEnabled(false);
    btn->setText("\xe2\x8f\xb3  Generazione...");
    const bool ok = regenCert();
    btn->setEnabled(true);
    btn->setText("\xf0\x9f\x94\x84  Rigenera certificato");
    if (ok) {
        const QString pin = readPin(QDir::homePath() + "/.prismalux/wan_cert.pin");
        fpEdit->setText(pin);
        QMessageBox::information(btn->window(), "Certificato rigenerato",
            "Nuovo certificato generato.\nFingerprint:\n" + pin +
            "\n\nDistribuisci il file wan_cert.pin ai worker come wan_server.pin.");
    } else {
        QMessageBox::warning(btn->window(), "Errore",
            "Impossibile rigenerare il certificato.\nVerifica che openssl sia installato.");
    }
}

static void secDoExportPin(QPushButton* btn) {
    const QString src = QDir::homePath() + "/.prismalux/wan_cert.pin";
    if (!QFileInfo::exists(src)) {
        QMessageBox::warning(btn->window(), "Pin non trovato",
            "wan_cert.pin non esiste.\nAvvia il server WAN con TLS per generarlo,\n"
            "oppure usa 'Rigenera certificato'.");
        return;
    }
    const QString dst = QFileDialog::getSaveFileName(
        btn->window(), "Esporta pin WAN",
        QDir::homePath() + "/wan_server.pin", "Pin files (*.pin);;All files (*)");
    if (dst.isEmpty()) return;
    QFile::remove(dst);
    if (QFile::copy(src, dst))
        QMessageBox::information(btn->window(), "Esportato",
            "Pin esportato in:\n" + dst +
            "\n\nCopialo sul worker come:\n~/.prismalux/wan_server.pin");
    else
        QMessageBox::warning(btn->window(), "Errore copia", "Impossibile scrivere il file.");
}

static void secDoImportPin(QPushButton* btn) {
    const QString src = QFileDialog::getOpenFileName(
        btn->window(), "Importa pin server WAN",
        QDir::homePath(), "Pin files (*.pin);;All files (*)");
    if (src.isEmpty()) return;
    const QString dst = pinCliPath();
    QDir().mkpath(QFileInfo(dst).absolutePath());
    QFile::remove(dst);
    if (QFile::copy(src, dst)) {
        QFile::setPermissions(dst,
            QFileDevice::ReadOwner | QFileDevice::WriteOwner);
        QMessageBox::information(btn->window(), "Importato",
            "Pin server importato.\nQuesto PC verificherà il certificato del server ad ogni connessione.");
    } else {
        QMessageBox::warning(btn->window(), "Errore", "Impossibile copiare il file.");
    }
}

/* ── Azioni WireGuard: genera chiavi, esporta conf ───────────── */
static void secDoGenWgKeys(QPushButton* btn, QLineEdit* pubkeyEdit) {
    auto check = ProcHelper::run("wg", {"--version"}, 3000);
    if (!check.ok) {
        QMessageBox::warning(btn->window(), "wg non trovato",
            "Installa WireGuard tools:\n  sudo apt install wireguard-tools");
        return;
    }
    if (QFileInfo::exists(QDir::homePath() + "/.prismalux/wireguard_private.key")) {
        const auto r = QMessageBox::question(btn->window(),
            "Chiavi esistenti",
            "Esistono già chiavi WireGuard.\nRigenerare (invaliderà le configurazioni esistenti)?");
        if (r != QMessageBox::Yes) return;
    }
    const QString privPath = QDir::homePath() + "/.prismalux/wireguard_private.key";
    QDir().mkpath(QFileInfo(privPath).absolutePath());

    auto privR = ProcHelper::run("wg", {"genkey"}, 5000);
    if (!privR.ok || privR.out.trimmed().isEmpty()) {
        QMessageBox::warning(btn->window(), "Errore", "Impossibile generare la chiave privata.");
        return;
    }
    const QString privKey = privR.out.trimmed();
    QSaveFile pf(privPath);
    if (!pf.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QMessageBox::warning(btn->window(), "Errore", "Impossibile scrivere la chiave privata.");
        return;
    }
    pf.write((privKey + "\n").toUtf8());
    pf.commit();
    QFile::setPermissions(privPath,
        QFileDevice::ReadOwner | QFileDevice::WriteOwner);

    /* Calcola pubkey dalla privkey via stdin (no shell injection) */
    auto pubR = ProcHelper::runWithInput("wg", {"pubkey"},
        (privKey + "\n").toUtf8(), 3000);
    if (!pubR.ok) {
        QMessageBox::warning(btn->window(), "Errore", "Impossibile calcolare la chiave pubblica.");
        return;
    }
    pubkeyEdit->setText(pubR.out.trimmed());
    QMessageBox::information(btn->window(), "Chiavi generate",
        "Chiave privata salvata in:\n" + privPath + " (0600)\n\n"
        "Chiave pubblica:\n" + pubR.out.trimmed() +
        "\n\nCondividi la pubkey con il server WireGuard.");
}

static void secDoExportWgConf(QPushButton* btn, QLineEdit* pubkeyEdit) {
    const QString privPath = QDir::homePath() + "/.prismalux/wireguard_private.key";
    if (!QFileInfo::exists(privPath)) {
        QMessageBox::warning(btn->window(), "Chiave privata mancante",
            "Genera prima le chiavi WireGuard.");
        return;
    }
    const QString pubkey = pubkeyEdit->text().trimmed();
    QString privKeyStr = "<leggi da " + privPath + ">";
    {
        QFile kf(privPath);
        if (kf.open(QIODevice::ReadOnly))
            privKeyStr = QString::fromUtf8(kf.readAll()).trimmed();
    }
    const QString tmpl = QString(
        "[Interface]\n"
        "# IP assegnato a questo worker nella VPN (es. 10.0.0.2/24)\n"
        "Address = 10.0.0.2/24\n"
        "PrivateKey = %1\n"
        "DNS = 1.1.1.1\n\n"
        "[Peer]\n"
        "# Pubkey del SERVER WireGuard\n"
        "PublicKey = <PUBKEY_SERVER>\n"
        "# IP:porta del server VPN su internet\n"
        "Endpoint = <IP_SERVER>:51820\n"
        "# Rotte: solo il traffico verso il master Prismalux (10.0.0.1) passa in VPN\n"
        "AllowedIPs = 10.0.0.1/32\n"
        "PersistentKeepalive = 25\n"
        "# Questo client (pubkey): %2\n"
    ).arg(
        privKeyStr,
        pubkey.isEmpty() ? "<pubkey non generata>" : pubkey
    );

    const QString dst = QFileDialog::getSaveFileName(
        btn->window(), "Salva wg0.conf",
        QDir::homePath() + "/wg0.conf", "WireGuard config (*.conf);;All files (*)");
    if (dst.isEmpty()) return;
    QSaveFile sf(dst);
    if (sf.open(QIODevice::WriteOnly | QIODevice::Text)) {
        sf.write(tmpl.toUtf8());
        sf.commit();
        QFile::setPermissions(dst,
            QFileDevice::ReadOwner | QFileDevice::WriteOwner);
        QMessageBox::information(btn->window(), "wg0.conf generato",
            "File salvato in:\n" + dst +
            "\n\nCompleta i campi <PUBKEY_SERVER> e <IP_SERVER>,\n"
            "poi: sudo wg-quick up " + dst);
    }
}

/* ══════════════════════════════════════════════════════════════
   buildSicurezzaWanTab
   ══════════════════════════════════════════════════════════════ */
QWidget* ImpostazioniPage::buildSicurezzaWanTab()
{
    auto* root = new QWidget;
    auto* rootLay = new QVBoxLayout(root);
    rootLay->setContentsMargins(12, 12, 12, 12);
    rootLay->setSpacing(16);

    /* ── Sezione 0: Guida rapida ─────────────────────────────── */
    auto* guideBox = new QGroupBox("\xf0\x9f\x93\x96  Guida rapida — come proteggere la rete WAN");
    auto* guideLay = new QVBoxLayout(guideBox);
    guideLay->setContentsMargins(6, 6, 6, 6);

    auto* guide = new QTextBrowser(guideBox);
    guide->setOpenExternalLinks(false);
    guide->setFrameShape(QFrame::NoFrame);
    guide->setMinimumHeight(dpiScale(320));
    guide->setReadOnly(true);
    guide->setHtml(
        "<style>"
        "body{font-size:13px; margin:4px;}"
        "h3{margin:10px 0 4px 0; color:#5b9bd5;}"
        "h4{margin:8px 0 2px 0; color:#7ec87e;}"
        "ol{margin:0 0 6px 18px; padding:0;}"
        "li{margin:3px 0;}"
        "code{background:#2a2a2a; color:#e0c97f; padding:1px 4px; border-radius:3px; font-size:12px;}"
        ".note{color:#aaa; font-size:12px; margin:4px 0 8px 0;}"
        ".warn{color:#e08060; font-size:12px;}"
        ".ok{color:#7ec87e; font-size:12px;}"
        "hr{border:none; border-top:1px solid #333; margin:10px 0;}"
        "</style>"

        "<h3>\xf0\x9f\x94\x92 Scenario A \xe2\x80\x94 Solo TLS (minimo consigliato)</h3>"
        "<p class='note'>Cifra il canale TCP della porta 11600 e impedisce connessioni da worker sconosciuti.</p>"

        "<h4>Sul PC SERVER (quello che avvia il calcolo distribuito):</h4>"
        "<ol>"
        "<li>Clicca <b>Rigenera certificato</b> \xe2\x86\x92 viene creato il certificato TLS in <code>~/.prismalux/</code></li>"
        "<li>Clicca <b>Esporta pin per worker</b> \xe2\x86\x92 salva il file <code>wan_server.pin</code> (es. su chiavetta USB o via SCP)</li>"
        "<li>Apri <b>Utility \xe2\x86\x92 LAN &amp; WAN \xe2\x86\x92 Server WAN</b>, assicurati che la casella <b>\xf0\x9f\x94\x92 TLS</b> sia attiva, poi clicca <b>Avvia</b></li>"
        "</ol>"

        "<h4>Su ogni PC WORKER (quelli che eseguono i task):</h4>"
        "<ol>"
        "<li>Apri <b>Impostazioni \xe2\x86\x92 Sicurezza WAN</b></li>"
        "<li>Clicca <b>Importa pin server</b> \xe2\x86\x92 seleziona il file <code>wan_server.pin</code> ricevuto dal server</li>"
        "<li>Apri <b>Utility \xe2\x86\x92 LAN &amp; WAN \xe2\x86\x92 Connetti al server</b>, attiva <b>\xf0\x9f\x94\x92 TLS</b> e inserisci IP:11600</li>"
        "</ol>"
        "<p class='ok'>\xe2\x9c\x94 Da questo momento il canale \xe2\x80\x8b\xc3\xa8 cifrato e il worker verifica l\xe2\x80\x99identit\xc3\xa0 del server.</p>"

        "<hr>"

        "<h3>\xf0\x9f\x9b\xa1 Scenario B \xe2\x80\x94 TLS + VPN WireGuard (consigliato per internet)</h3>"
        "<p class='note'>WireGuard crea una rete privata cifrata tra tutti i PC. "
        "Il server Prismalux \xc3\xa8 raggiungibile solo dall\xe2\x80\x99interno della VPN, non da internet diretto.</p>"

        "<h4>Prerequisiti (su OGNI PC coinvolto):</h4>"
        "<ol>"
        "<li>Installa WireGuard: <code>sudo apt install wireguard-tools</code></li>"
        "<li>Il server VPN pu\xc3\xb2 essere il tuo router (se supporta WireGuard), un VPS, "
        "o uno dei PC del cluster (scelto come hub)</li>"
        "</ol>"

        "<h4>Su ogni PC WORKER \xe2\x80\x94 genera le chiavi:</h4>"
        "<ol>"
        "<li>Apri <b>Impostazioni \xe2\x86\x92 Sicurezza WAN \xe2\x86\x92 sezione WireGuard</b></li>"
        "<li>Clicca <b>Genera chiavi WireGuard</b> \xe2\x86\x92 crea la coppia privkey/pubkey</li>"
        "<li>Copia la <b>Chiave pubblica</b> mostrata (pulsante \xf0\x9f\x93\x8b) \xe2\x86\x92 mandala all\xe2\x80\x99amministratore del server VPN</li>"
        "<li>Clicca <b>Genera wg0.conf</b> \xe2\x86\x92 salva il file e compila i campi mancanti:"
        "<ul>"
        "<li><code>PublicKey</code> del server VPN (fornita dall\xe2\x80\x99amministratore)</li>"
        "<li><code>Endpoint</code> = IP pubblico del server VPN : porta (es. <code>203.0.113.1:51820</code>)</li>"
        "<li><code>Address</code> = IP privato assegnato a questo worker (es. <code>10.0.0.2/24</code>)</li>"
        "</ul></li>"
        "<li>Attiva la VPN: <code>sudo wg-quick up /percorso/wg0.conf</code></li>"
        "</ol>"

        "<h4>Sul SERVER VPN \xe2\x80\x94 aggiungi ogni worker:</h4>"
        "<ol>"
        "<li>Nel file di configurazione del server WireGuard aggiungi un blocco per ogni worker:<br>"
        "<code>[Peer]<br>PublicKey = &lt;PUBKEY_DEL_WORKER&gt;<br>AllowedIPs = 10.0.0.2/32</code></li>"
        "<li>Ricarica la config: <code>sudo wg syncconf wg0 &lt;(wg-quick strip wg0)</code></li>"
        "</ol>"

        "<h4>Sul PC MASTER (server Prismalux):</h4>"
        "<ol>"
        "<li>Attiva anche lui la VPN (stesso procedimento worker, con il suo IP, es. <code>10.0.0.1/24</code>)</li>"
        "<li>Avvia il server WAN sulla porta 11600 \xe2\x80\x94 i worker si connettono usando <code>10.0.0.1:11600</code></li>"
        "<li>Facoltativo ma consigliato: attiva anche il <b>TLS</b> (Scenario A) per doppia protezione</li>"
        "</ol>"

        "<p class='ok'>\xe2\x9c\x94 I worker vedono solo la rete VPN privata \xe2\x80\x94 la porta 11600 non \xc3\xa8 mai esposta a internet.</p>"
        "<p class='warn'>\xe2\x9a\xa0 Se un worker si disconnette dalla VPN, Prismalux mostra il nodo offline e riassegna i task.</p>"

        "<hr>"
        "<p class='note'><b>Quando serve rigenerare il certificato TLS?</b> "
        "Solo se sospetti che la chiave privata sia stata compromessa, o ogni 10 anni (il cert dura 3650 giorni). "
        "Dopo ogni rigenera, ripeti il passaggio \xe2\x80\x9cEsporta pin\xe2\x80\x9d su tutti i worker.</p>"
    );

    guideLay->addWidget(guide);
    rootLay->addWidget(guideBox);

    /* ── Sezione 1: Certificato TLS WAN ──────────────────────── */
    auto* certBox = new QGroupBox("\xf0\x9f\x94\x92  Certificato TLS WAN (porta 11600)");
    auto* certLay = new QVBoxLayout(certBox);
    certLay->setSpacing(8);

    auto* fpRow = new QWidget;
    auto* fpLay = new QHBoxLayout(fpRow);
    fpLay->setContentsMargins(0,0,0,0); fpLay->setSpacing(6);
    auto* fpLbl = new QLabel("Fingerprint (SHA-256):", fpRow);
    fpLbl->setFixedWidth(dpiScale(160));
    auto* fpEdit = new QLineEdit(fpRow);
    fpEdit->setReadOnly(true);
    fpEdit->setFont(QFont("monospace"));
    fpEdit->setPlaceholderText("(server non avviato con TLS / certificato non generato)");
    fpEdit->setText(readPin(pinSrvPath()));
    auto* fpCopyBtn = new QPushButton("\xf0\x9f\x93\x8b", fpRow);
    fpCopyBtn->setFixedWidth(dpiScale(32));
    fpCopyBtn->setToolTip("Copia fingerprint negli appunti");
    connect(fpCopyBtn, &QPushButton::clicked, fpCopyBtn, [fpEdit]() {
        QApplication::clipboard()->setText(fpEdit->text());
    });
    fpLay->addWidget(fpLbl);
    fpLay->addWidget(fpEdit, 1);
    fpLay->addWidget(fpCopyBtn);
    certLay->addWidget(fpRow);

    auto* certInfoLbl = new QLabel(
        "<small style='color:#888;'>"
        "Il fingerprint viene generato all\xe2\x80\x99 avvio del server WAN con TLS attivo.<br>"
        "Per rigenerarlo manualmente: clicca \xe2\x80\x9cRigenera certificato\xe2\x80\x9d qui sotto.<br>"
        "<b>Dopo la rigenerazione devi ridistribuire il pin a tutti i worker.</b>"
        "</small>");
    certInfoLbl->setWordWrap(true);
    certInfoLbl->setTextFormat(Qt::RichText);
    certLay->addWidget(certInfoLbl);

    auto* certBtnRow = new QWidget;
    auto* certBtnLay = new QHBoxLayout(certBtnRow);
    certBtnLay->setContentsMargins(0,0,0,0); certBtnLay->setSpacing(8);

    auto* regenBtn = new QPushButton("\xf0\x9f\x94\x84  Rigenera certificato");
    regenBtn->setToolTip(
        "Elimina server.crt e server.key e ne genera una nuova coppia.\n"
        "Aggiorna automaticamente wan_cert.pin.\n"
        "ATTENZIONE: i worker con il vecchio pin non potranno più connettersi.");
    connect(regenBtn, &QPushButton::clicked, regenBtn, [regenBtn, fpEdit](){
        secDoRegenCert(regenBtn, fpEdit);
    });

    auto* exportPinBtn = new QPushButton("\xf0\x9f\x93\xa4  Esporta pin per worker");
    exportPinBtn->setToolTip(
        "Salva wan_cert.pin in una posizione a scelta.\n"
        "Copialo sul worker come ~/.prismalux/wan_server.pin");
    connect(exportPinBtn, &QPushButton::clicked, exportPinBtn, [exportPinBtn](){
        secDoExportPin(exportPinBtn);
    });

    auto* importPinBtn = new QPushButton("\xf0\x9f\x93\xa5  Importa pin server (lato worker)");
    importPinBtn->setToolTip(
        "Carica il pin del server su questo PC come wan_server.pin.\n"
        "Da usare quando questo PC è il WORKER che si connette a un server remoto.");
    connect(importPinBtn, &QPushButton::clicked, importPinBtn, [importPinBtn](){
        secDoImportPin(importPinBtn);
    });

    certBtnLay->addWidget(regenBtn);
    certBtnLay->addWidget(exportPinBtn);
    certBtnLay->addWidget(importPinBtn);
    certBtnLay->addStretch();
    certLay->addWidget(certBtnRow);

    /* Pin worker corrente */
    auto* cliPinRow = new QWidget;
    auto* cliPinLay = new QHBoxLayout(cliPinRow);
    cliPinLay->setContentsMargins(0,0,0,0); cliPinLay->setSpacing(6);
    auto* cliPinLbl = new QLabel("Pin server (worker):", cliPinRow);
    cliPinLbl->setFixedWidth(dpiScale(160));
    auto* cliPinEdit = new QLineEdit(cliPinRow);
    cliPinEdit->setReadOnly(true);
    cliPinEdit->setFont(QFont("monospace"));
    cliPinEdit->setPlaceholderText("(nessun pin importato — accetta qualsiasi certificato)");
    cliPinEdit->setText(readPin(pinCliPath()));
    auto* cliPinClearBtn = new QPushButton("\xf0\x9f\x97\x91", cliPinRow);
    cliPinClearBtn->setFixedWidth(dpiScale(32));
    cliPinClearBtn->setToolTip("Rimuovi pin worker (torna a VerifyNone)");
    connect(cliPinClearBtn, &QPushButton::clicked, cliPinClearBtn, [cliPinEdit]() {
        QFile::remove(pinCliPath());
        cliPinEdit->clear();
    });
    cliPinLay->addWidget(cliPinLbl);
    cliPinLay->addWidget(cliPinEdit, 1);
    cliPinLay->addWidget(cliPinClearBtn);
    certLay->addWidget(cliPinRow);

    rootLay->addWidget(certBox);

    /* ── Sezione 2: Chiavi WireGuard VPN ─────────────────────── */
    auto* vpnBox = new QGroupBox("\xf0\x9f\x9b\xa1  Chiavi WireGuard VPN");
    auto* vpnLay = new QVBoxLayout(vpnBox);
    vpnLay->setSpacing(8);

    auto* vpnInfoLbl = new QLabel(
        "<small style='color:#888;'>"
        "WireGuard cifra tutto il traffico WAN a livello di rete — "
        "complementare al TLS della porta 11600.<br>"
        "Genera una coppia di chiavi, condividi la pubkey con il server VPN "
        "e usa la chiave privata nel file wg0.conf del worker.<br>"
        "<b>La chiave privata viene salvata in ~/.prismalux/wireguard_private.key (0600)</b>."
        "</small>");
    vpnInfoLbl->setWordWrap(true);
    vpnInfoLbl->setTextFormat(Qt::RichText);
    vpnLay->addWidget(vpnInfoLbl);

    /* Pubkey display */
    auto* pubkeyRow = new QWidget;
    auto* pubkeyLay = new QHBoxLayout(pubkeyRow);
    pubkeyLay->setContentsMargins(0,0,0,0); pubkeyLay->setSpacing(6);
    auto* pubkeyLbl = new QLabel("Chiave pubblica:", pubkeyRow);
    pubkeyLbl->setFixedWidth(dpiScale(160));
    auto* pubkeyEdit = new QLineEdit(pubkeyRow);
    pubkeyEdit->setReadOnly(true);
    pubkeyEdit->setFont(QFont("monospace"));
    pubkeyEdit->setPlaceholderText("(genera una coppia di chiavi)");
    /* Carica pubkey se la privkey esiste già */
    const QString privKeyPath = QDir::homePath() + "/.prismalux/wireguard_private.key";
    if (QFileInfo::exists(privKeyPath)) {
        QFile kf(privKeyPath);
        if (kf.open(QIODevice::ReadOnly)) {
            const QByteArray privData = kf.readAll();
            kf.close();
            auto r = ProcHelper::runWithInput("wg", {"pubkey"}, privData, 3000);
            if (r.ok) pubkeyEdit->setText(r.out.trimmed());
        }
    }
    auto* pubkeyCopyBtn = new QPushButton("\xf0\x9f\x93\x8b", pubkeyRow);
    pubkeyCopyBtn->setFixedWidth(dpiScale(32));
    pubkeyCopyBtn->setToolTip("Copia pubkey");
    connect(pubkeyCopyBtn, &QPushButton::clicked, pubkeyCopyBtn, [pubkeyEdit]() {
        QApplication::clipboard()->setText(pubkeyEdit->text());
    });
    pubkeyLay->addWidget(pubkeyLbl);
    pubkeyLay->addWidget(pubkeyEdit, 1);
    pubkeyLay->addWidget(pubkeyCopyBtn);
    vpnLay->addWidget(pubkeyRow);

    auto* vpnBtnRow = new QWidget;
    auto* vpnBtnLay = new QHBoxLayout(vpnBtnRow);
    vpnBtnLay->setContentsMargins(0,0,0,0); vpnBtnLay->setSpacing(8);

    auto* genKeysBtn = new QPushButton("\xf0\x9f\x94\x84  Genera chiavi WireGuard");
    genKeysBtn->setToolTip("Richiede 'wg' installato (sudo apt install wireguard-tools)");
    connect(genKeysBtn, &QPushButton::clicked, genKeysBtn, [genKeysBtn, pubkeyEdit](){
        secDoGenWgKeys(genKeysBtn, pubkeyEdit);
    });

    auto* exportConfBtn = new QPushButton("\xf0\x9f\x93\x84  Genera wg0.conf (worker)");
    exportConfBtn->setToolTip(
        "Genera un file di configurazione WireGuard client da completare.\n"
        "Richiede: IP del server VPN, pubkey del server, IP assegnato al worker.");
    connect(exportConfBtn, &QPushButton::clicked, exportConfBtn, [exportConfBtn, pubkeyEdit](){
        secDoExportWgConf(exportConfBtn, pubkeyEdit);
    });

    vpnBtnLay->addWidget(genKeysBtn);
    vpnBtnLay->addWidget(exportConfBtn);
    vpnBtnLay->addStretch();
    vpnLay->addWidget(vpnBtnRow);

    rootLay->addWidget(vpnBox);
    rootLay->addStretch();

    auto* sc = new QScrollArea;
    sc->setFrameShape(QFrame::NoFrame);
    sc->setWidgetResizable(true);
    sc->setWidget(root);
    return sc;
}
