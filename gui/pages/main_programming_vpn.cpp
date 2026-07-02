/* ======================================================================
   main_programming_vpn.cpp — VPN & Tunnel slot di ProgrammazionePage
   Estratto da main_programming_slots.cpp.
   ====================================================================== */
#include "main_programming.h"
#include "../prismalux_paths.h"
#include "../log_bus.h"
#include "../dpi_utils.h"

#include <QApplication>
#include <QCheckBox>
#include <QClipboard>
#include <QComboBox>
#include <QDir>
#include <QFile>
#include <QFileDialog>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QNetworkInterface>
#include <QAbstractSocket>
#include <QPlainTextEdit>
#include <QProcess>
#include <QPushButton>
#include <QRandomGenerator>
#include <QRegularExpression>
#include <QSettings>
#include <QSpinBox>
#include <QTextEdit>
#include <QTimer>
#include <QDesktopServices>
#include <QUrl>

namespace P = PrismaluxPaths;

static const char* kVpnTemplates[] = {
    "# WireGuard — configurazione client\n"
    "# Salva come /etc/wireguard/wg0.conf e usa: sudo wg-quick up wg0\n"
    "\n"
    "[Interface]\n"
    "Address = 10.0.0.2/24\n"
    "PrivateKey = <CHIAVE_PRIVATA_CLIENT>\n"
    "DNS = 1.1.1.1\n"
    "\n"
    "[Peer]\n"
    "PublicKey = <CHIAVE_PUBBLICA_SERVER>\n"
    "AllowedIPs = 0.0.0.0/0, ::/0\n"
    "Endpoint = <SERVER_IP>:51820\n"
    "PersistentKeepalive = 25\n"
    "\n"
    "# Genera le chiavi con:\n"
    "#   wg genkey | tee privatekey | wg pubkey > publickey\n",

    "# OpenVPN — configurazione client (.ovpn)\n"
    "# Usa: sudo openvpn --config client.ovpn\n"
    "\n"
    "client\n"
    "dev tun\n"
    "proto udp\n"
    "remote <SERVER_IP> 1194\n"
    "resolv-retry infinite\n"
    "nobind\n"
    "persist-key\n"
    "persist-tun\n"
    "cipher AES-256-CBC\n"
    "auth SHA256\n"
    "verb 3\n"
    "keepalive 10 120\n"
    "\n"
    "<ca>\n"
    "# Incolla il contenuto di ca.crt\n"
    "</ca>\n"
    "<cert>\n"
    "# Incolla il contenuto di client.crt\n"
    "</cert>\n"
    "<key>\n"
    "# Incolla il contenuto di client.key\n"
    "</key>\n",

    "# SSH Tunnel — esempi pronti all'uso\n"
    "\n"
    "# 1. Tunnel LOCALE: porta locale 8080 -> server:80\n"
    "ssh -N -L 8080:localhost:80 utente@<SERVER>\n"
    "\n"
    "# 2. Tunnel REMOTO: porta remota 9090 -> localhost:80\n"
    "ssh -N -R 9090:localhost:80 utente@<SERVER>\n"
    "\n"
    "# 3. SOCKS proxy dinamico (porta 1080)\n"
    "ssh -N -D 1080 -C utente@<SERVER>\n"
    "\n"
    "# 4. Sessione persistente con autossh\n"
    "#    sudo apt install autossh\n"
    "autossh -M 20000 -N -L 8080:localhost:80 utente@<SERVER>\n"
    "\n"
    "# Configura in /etc/ssh/sshd_config del server:\n"
    "#   AllowTcpForwarding yes\n"
    "#   GatewayPorts yes       # solo per tunnel remoti\n",

    "#!/bin/bash\n"
    "# Wi-Fi Hotspot con NetworkManager\n"
    "# Adatta SSID, password e interfaccia (wlan0 / wlp3s0)\n"
    "\n"
    "IFACE=\"wlan0\"\n"
    "SSID=\"PrismaluxNet\"\n"
    "PASSWORD=\"password_sicura\"\n"
    "CON_NAME=\"PrismaluxAP\"\n"
    "\n"
    "nmcli con add type wifi ifname \"$IFACE\" con-name \"$CON_NAME\" \\\n"
    "  ssid \"$SSID\" mode ap\n"
    "\n"
    "nmcli con modify \"$CON_NAME\" \\\n"
    "  wifi.band bg wifi.channel 6 \\\n"
    "  wifi-sec.key-mgmt wpa-psk wifi-sec.psk \"$PASSWORD\"\n"
    "\n"
    "nmcli con modify \"$CON_NAME\" ipv4.method shared\n"
    "nmcli con up \"$CON_NAME\"\n"
    "echo \"Hotspot '$SSID' attivo su $IFACE\"\n"
    "\n"
    "# Per fermare:\n"
    "# nmcli con down \"$CON_NAME\"\n",

    /* idx 4 — n2n Supernode */
    "#!/bin/bash\n"
    "# n2n Supernode — nodo centrale WAN Prismalux (scritto in C)\n"
    "# Installa: sudo apt install n2n\n"
    "# Avvia sul server: sudo bash /tmp/prismalux_n2n_supernode.sh\n"
    "# Apri nel firewall: sudo ufw allow 7654/udp\n"
    "\n"
    "SUPERNODE_PORT=7654\n"
    "\n"
    "if ! command -v supernode &>/dev/null; then\n"
    "    echo '[n2n] Installo n2n...'\n"
    "    apt-get install -y n2n\n"
    "fi\n"
    "\n"
    "echo \"[Prismalux] Avvio supernode n2n su UDP $SUPERNODE_PORT...\"\n"
    "supernode -p \"$SUPERNODE_PORT\" -v\n"
    "\n"
    "# Dopo aver avviato il supernode, connetti anche questo host come edge\n"
    "# (IP server nella VPN = 10.10.0.1):\n"
    "# sudo edge -c prismalux_wan -k <PSK> -a 10.10.0.1/24 -l localhost:$SUPERNODE_PORT -f\n",

    /* idx 5 — n2n Edge (worker WAN) */
    "#!/bin/bash\n"
    "# n2n Edge — worker WAN Prismalux (esegui su ogni nodo remoto)\n"
    "# Installa: sudo apt install n2n\n"
    "# Avvia: sudo bash prismalux_n2n_edge.sh\n"
    "# Clicca 'Genera chiavi n2n' per riempire COMMUNITY e PSK automaticamente.\n"
    "\n"
    "SUPERNODE_IP=\"<SERVER_IP>\"\n"
    "SUPERNODE_PORT=7654\n"
    "COMMUNITY=\"prismalux_wan\"\n"
    "PSK=\"<CHIAVE_CONDIVISA_AES256>\"\n"
    "EDGE_IP=\"10.10.0.2/24\"\n"
    "# Cambia EDGE_IP per ogni worker: 10.10.0.3/24, 10.10.0.4/24 ...\n"
    "\n"
    "if ! command -v edge &>/dev/null; then\n"
    "    echo '[n2n] Installo n2n...'\n"
    "    apt-get install -y n2n\n"
    "fi\n"
    "\n"
    "echo \"[Prismalux] Connessione al supernode $SUPERNODE_IP:$SUPERNODE_PORT...\"\n"
    "edge -c \"$COMMUNITY\" \\\n"
    "     -k \"$PSK\" \\\n"
    "     -a \"$EDGE_IP\" \\\n"
    "     -l \"$SUPERNODE_IP:$SUPERNODE_PORT\" \\\n"
    "     -f\n"
    "\n"
    "# Il WAN Compute Prismalux (porta 11600) sara' raggiungibile via VPN:\n"
    "#   Server: 10.10.0.1:11600  |  Questo nodo: 10.10.0.2\n",
};

void ProgrammazionePage::onVpnTypeChanged(int idx)
{
    if (!m_vpnConfig) return;
    if (idx >= 0 && idx < 6)
        m_vpnConfig->setPlainText(QString::fromUtf8(kVpnTemplates[idx]));
    if (m_vpnGenKeysBtn)
        m_vpnGenKeysBtn->setVisible(idx >= 4);
}

void ProgrammazionePage::onVpnGenN2nKeys()
{
    if (!m_vpnConfig || !m_vpnLog) return;

    const QString alphanum = QStringLiteral("abcdefghijklmnopqrstuvwxyz0123456789");
    QString community = QStringLiteral("plx_");
    for (int i = 0; i < 8; ++i)
        community += alphanum[QRandomGenerator::global()->bounded(alphanum.size())];

    QString psk;
    for (int i = 0; i < 32; ++i)
        psk += QString::number(QRandomGenerator::global()->bounded(16), 16);

    QString cfg = m_vpnConfig->toPlainText();
    cfg.replace(QStringLiteral("prismalux_wan"), community);
    cfg.replace(QStringLiteral("<CHIAVE_CONDIVISA_AES256>"), psk);
    m_vpnConfig->setPlainText(cfg);

    m_vpnLog->append(
        QString("<span style='color:#4ade80;'>"
                "\xf0\x9f\x94\x91  Chiavi n2n generate:<br>"
                "&nbsp; Community: <b>%1</b><br>"
                "&nbsp; PSK&nbsp;&nbsp;&nbsp;&nbsp;: <b>%2</b><br>"
                "Usa <b>le stesse</b> su ogni nodo edge e sul server.</span>")
            .arg(community.toHtmlEscaped())
            .arg(psk.toHtmlEscaped()));

    LogBus::post(QString("\xf0\x9f\x94\x91 n2n — chiavi generate. Community: %1").arg(community));
}

void ProgrammazionePage::onVpnGenerateClicked()
{
    if (!m_vpnConfig || !m_vpnLog || !m_vpnStatusLbl) return;

    const QString config  = m_vpnConfig->toPlainText().trimmed();
    const QString tipoPkg = m_vpnTypeCombo
        ? m_vpnTypeCombo->currentText() : QStringLiteral("VPN");

    m_vpnStatusLbl->setText(tr("\xf0\x9f\xa4\x96  AI in elaborazione..."));
    m_vpnLog->append(
        QString("<span style='color:#94a3b8;'>\xf0\x9f\xa4\x96  "
                "Invio configurazione %1 all'AI...</span>").arg(tipoPkg));

    const QString sys =
        "Sei un esperto di reti e sicurezza informatica su Linux. "
        "Analizza la configurazione " + tipoPkg + " che ti fornisco, "
        "identifica eventuali problemi di sicurezza o configurazioni mancanti, "
        "e restituisci una versione migliorata e completa con commenti esplicativi. "
        "Rispondi con solo il file di configurazione migliorato, senza spiegazioni aggiuntive.";

    disconnect(m_vpnAiTokenConn);
    disconnect(m_vpnAiFinishedConn);
    disconnect(m_vpnAiErrorConn);

    m_vpnConfig->clear();

    m_vpnAiTokenConn = connect(m_ai, &AiClient::token,
                               this, &ProgrammazionePage::onVpnAiToken);
    m_vpnAiFinishedConn = connect(m_ai, &AiClient::finished,
                                  this, &ProgrammazionePage::onVpnAiFinished);
    m_vpnAiErrorConn = connect(m_ai, &AiClient::error,
                               this, &ProgrammazionePage::onVpnAiError);

    m_ai->chat(sys, config);
}

void ProgrammazionePage::onVpnAiToken(const QString& t)
{
    if (!m_vpnConfig) return;
    QTextCursor c = m_vpnConfig->textCursor();
    c.movePosition(QTextCursor::End);
    c.insertText(t);
    m_vpnConfig->setTextCursor(c);
}

void ProgrammazionePage::onVpnAiFinished(const QString& /*full*/)
{
    disconnect(m_vpnAiTokenConn);
    disconnect(m_vpnAiFinishedConn);
    disconnect(m_vpnAiErrorConn);
    if (m_vpnStatusLbl)
        m_vpnStatusLbl->setText(tr("\xe2\x9c\x85  Config aggiornata dall'AI"));
    m_vpnLog->append(
        "<span style='color:#4ade80;'>\xe2\x9c\x85  Configurazione migliorata dall'AI.</span>");
}

void ProgrammazionePage::onVpnAiError(const QString& msg)
{
    disconnect(m_vpnAiTokenConn);
    disconnect(m_vpnAiFinishedConn);
    disconnect(m_vpnAiErrorConn);
    if (m_vpnStatusLbl)
        m_vpnStatusLbl->setText(tr("\xe2\x9d\x8c  Errore AI"));
    m_vpnLog->append(
        "<span style='color:#f87171;'>\xe2\x9d\x8c  " + msg.toHtmlEscaped() + "</span>");
}

/* Rileva le interfacce VPN attive (nomi wg, tun, tap, ppp, n2n) e il loro IPv4,
   usando QNetworkInterface: nessun processo esterno, nessun privilegio root. */
void ProgrammazionePage::vpnRefreshStatus()
{
    if (!m_vpnLiveStatusLbl) return;

    QStringList active;
    const auto ifaces = QNetworkInterface::allInterfaces();
    for (const QNetworkInterface& ni : ifaces) {
        const QString n = ni.name();
        const bool isVpn = n.startsWith("wg")  || n.startsWith("tun") ||
                           n.startsWith("tap") || n.startsWith("ppp") || n.startsWith("n2n");
        if (!isVpn) continue;
        const auto flags = ni.flags();
        if (!flags.testFlag(QNetworkInterface::IsUp) ||
            !flags.testFlag(QNetworkInterface::IsRunning))
            continue;
        QString ip;
        for (const QNetworkAddressEntry& e : ni.addressEntries()) {
            if (e.ip().protocol() == QAbstractSocket::IPv4Protocol) {
                ip = e.ip().toString();
                break;
            }
        }
        active << QString("%1 (%2)").arg(n, ip.isEmpty() ? tr("nessun IP") : ip);
    }

    if (active.isEmpty()) {
        m_vpnLiveStatusLbl->setText(tr("\xe2\x9a\xaa  Nessuna VPN attiva"));
        m_vpnLiveStatusLbl->setStyleSheet("color:#94a3b8;");
    } else {
        m_vpnLiveStatusLbl->setText(QString::fromUtf8("\xf0\x9f\x9f\xa2  ") + active.join(", "));
        m_vpnLiveStatusLbl->setStyleSheet("color:#22c55e;");
    }
}

void ProgrammazionePage::onVpnTestClicked()
{
    vpnRefreshStatus();
    if (m_vpnLog && m_vpnLiveStatusLbl)
        m_vpnLog->append(QString::fromUtf8("\xf0\x9f\x94\x8d  ") +
                         tr("Stato VPN: ") + m_vpnLiveStatusLbl->text());
}

void ProgrammazionePage::onVpnApplyClicked()
{
    if (!m_vpnConfig || !m_vpnLog || !m_vpnTypeCombo) return;

    const int    idx    = m_vpnTypeCombo->currentIndex();
    const QString cfg   = m_vpnConfig->toPlainText().trimmed();

    if (cfg.isEmpty()) {
        m_vpnLog->append("<span style='color:#f87171;'>\xe2\x9d\x8c  Configurazione vuota.</span>");
        return;
    }

    /* SSH: copia il comando negli appunti e suggerisce di eseguirlo in terminale */
    if (idx == 2) {
        qApp->clipboard()->setText(cfg);
        m_vpnLog->append(
            "\xf0\x9f\x93\x8b  Comandi SSH copiati negli appunti.<br>"
            "<span style='color:#94a3b8;'>Incolla in un terminale per eseguirli.</span>");
        if (m_vpnStatusLbl)
            m_vpnStatusLbl->setText(tr("\xf0\x9f\x93\x8b  Copiato negli appunti"));
        return;
    }

    /* n2n Edge: gira sul nodo remoto — copia script negli appunti */
    if (idx == 5) {
        if (cfg.contains("<SERVER_IP>") || cfg.contains("<CHIAVE_CONDIVISA_AES256>")) {
            m_vpnLog->append(
                "<span style='color:#f87171;'>\xe2\x9d\x8c  "
                "Sostituisci &lt;SERVER_IP&gt; e genera le chiavi prima di procedere.</span>");
            LogBus::post("\xe2\x9d\x8c VPN n2n edge: configura SERVER_IP e chiavi prima di applicare.");
            return;
        }
        qApp->clipboard()->setText(cfg);
        m_vpnLog->append(
            "\xf0\x9f\x93\x8b  Script edge n2n copiato negli appunti.<br>"
            "<span style='color:#94a3b8;'>Incolla e avvia con "
            "<b>sudo bash</b> su ogni nodo worker remoto.</span>");
        if (m_vpnStatusLbl)
            m_vpnStatusLbl->setText(tr("\xf0\x9f\x93\x8b  Copiato negli appunti"));
        return;
    }

    /* Determina file temporaneo e comando */
    QString tmpPath, cmd;
    QStringList args;

    if (idx == 0) {
        /* WireGuard */
        tmpPath = P::tmpDir() + "wg0.conf";
        cmd = "bash";
        args = {"-c",
                "cp " + tmpPath + " /etc/wireguard/wg0.conf && "
                "wg-quick up wg0 && echo 'WireGuard attivato.'"};
    } else if (idx == 1) {
        /* OpenVPN */
        tmpPath = P::tmpDir() + "client.ovpn";
        cmd = "openvpn";
        args = {"--config", tmpPath};
    } else if (idx == 4) {
        /* n2n Supernode — avvia localmente con pkexec */
        tmpPath = P::tmpDir() + "n2n_supernode.sh";
        cmd = "bash";
        args = {tmpPath};
    } else {
        /* Hotspot script bash (idx == 3) */
        tmpPath = P::tmpDir() + "hotspot.sh";
        cmd = "bash";
        args = {tmpPath};
    }

    /* Scrivi il file temporaneo */
    QFile tf(tmpPath);
    if (!tf.open(QIODevice::WriteOnly | QIODevice::Text)) {
        m_vpnLog->append(
            "<span style='color:#f87171;'>\xe2\x9d\x8c  Impossibile scrivere "
            + tmpPath.toHtmlEscaped() + "</span>");
        return;
    }
    tf.write(cfg.toUtf8());
    tf.close();

    m_vpnLog->append(
        QString("<span style='color:#94a3b8;'>\xe2\x96\xb6  pkexec %1 %2</span>")
            .arg(cmd).arg(args.join(" ")));

    if (!m_vpnProc) {
        m_vpnProc = new QProcess(this);
        m_vpnProc->setProcessChannelMode(QProcess::MergedChannels);
        connect(m_vpnProc, &QProcess::readyRead,
                this, &ProgrammazionePage::onVpnProcReadyRead);
        connect(m_vpnProc,
                QOverload<int,QProcess::ExitStatus>::of(&QProcess::finished),
                this, &ProgrammazionePage::onVpnProcFinished);
        connect(m_vpnProc, &QProcess::errorOccurred,
                this, [this](QProcess::ProcessError err) {
            if (err == QProcess::FailedToStart)
                qWarning() << "[vpn_apply] processo non avviato:" << m_vpnProc->program();
        });
    }

    QStringList pkArgs = {cmd};
    pkArgs += args;
    m_vpnProc->start("pkexec", pkArgs);
    if (m_vpnStatusLbl) m_vpnStatusLbl->setText(tr("\xf0\x9f\x94\x84  Esecuzione in corso..."));
}

void ProgrammazionePage::onVpnStopClicked()
{
    m_ai->abort();
    if (m_vpnProc && m_vpnProc->state() != QProcess::NotRunning) {
        m_vpnProc->terminate();
        m_vpnProc->waitForFinished(2000);
    }
    m_vpnValidating = false;
    if (m_vpnValidateBtn) m_vpnValidateBtn->setEnabled(true);
    if (m_vpnStatusLbl) m_vpnStatusLbl->setText(tr("\xe2\x8f\xb9  Fermato"));
}

/* ── Valida config — simulazione senza root ── */
void ProgrammazionePage::onVpnValidateClicked()
{
    if (!m_vpnConfig || !m_vpnLog || !m_vpnTypeCombo) return;
    if (m_vpnProc && m_vpnProc->state() != QProcess::NotRunning) {
        m_vpnLog->append(
            "<span style='color:#f87171;'>\xe2\x9d\x8c  "
            "Un processo VPN e' gia' in esecuzione.</span>");
        return;
    }

    const QString cfg = m_vpnConfig->toPlainText().trimmed();
    if (cfg.isEmpty()) {
        m_vpnLog->append(
            "<span style='color:#f87171;'>\xe2\x9d\x8c  Configurazione vuota.</span>");
        return;
    }

    /* Scrivi config in file temp */
    const QString tmpCfg = P::tmpDir() + "vpn_validate.cfg";
    QFile fc(tmpCfg);
    if (!fc.open(QIODevice::WriteOnly | QIODevice::Text)) {
        m_vpnLog->append(
            "<span style='color:#f87171;'>\xe2\x9d\x8c  "
            "Impossibile creare il file temporaneo.</span>");
        return;
    }
    fc.write(cfg.toUtf8());
    fc.close();

    /* Tipo corrente come stringa */
    static const char* kTypes[] = {
        "wireguard", "openvpn", "ssh", "hotspot", "n2n_supernode", "n2n_edge"
    };
    const int idx = m_vpnTypeCombo->currentIndex();
    const QString type = (idx >= 0 && idx < 6)
        ? QString::fromUtf8(kTypes[idx]) : QStringLiteral("unknown");

    /* Script bash di validazione (no root) */
    const QString script =
        "#!/bin/bash\n"
        "CFG=\"" + tmpCfg + "\"\n"
        "TYPE=\"" + type + "\"\n"
        "PASS=0; FAIL=0; WARN=0\n"
        "ok()   { echo \"[OK]  $1\"; ((PASS++)); }\n"
        "err()  { echo \"[ERR] $1\"; ((FAIL++)); }\n"
        "warn() { echo \"[WRN] $1\"; ((WARN++)); }\n"
        "\n"
        "# 1. Placeholder\n"
        "PH=$(grep -oP '<[A-Z_]+>' \"$CFG\" | sort -u | tr '\\n' ' ')\n"
        "if [ -z \"$PH\" ]; then ok \"Nessun placeholder da sostituire\";\n"
        "else err \"Placeholder non sostituiti: $PH\"; fi\n"
        "\n"
        "# 2. Binario richiesto\n"
        "case \"$TYPE\" in\n"
        "  wireguard)     BIN=wg;         PKG=wireguard ;;\n"
        "  openvpn)       BIN=openvpn;    PKG=openvpn ;;\n"
        "  ssh)           BIN=ssh;        PKG=openssh-client ;;\n"
        "  hotspot)       BIN=nmcli;      PKG=network-manager ;;\n"
        "  n2n_supernode) BIN=supernode;  PKG=n2n ;;\n"
        "  n2n_edge)      BIN=edge;       PKG=n2n ;;\n"
        "  *)             BIN=; PKG= ;;\n"
        "esac\n"
        "if [ -n \"$BIN\" ]; then\n"
        "  if command -v \"$BIN\" &>/dev/null;\n"
        "  then ok \"Binario trovato: $(command -v $BIN)\";\n"
        "  else err \"Binario mancante: $BIN  (installa: sudo apt install $PKG)\"; fi\n"
        "fi\n"
        "\n"
        "# 3. Porta locale libera\n"
        "if [ \"$TYPE\" = \"n2n_supernode\" ]; then\n"
        "  if ss -lun 2>/dev/null | grep -q ':7654 ';\n"
        "  then err \"Porta UDP 7654 gia' in uso\";\n"
        "  else ok \"Porta UDP 7654 libera\"; fi\n"
        "fi\n"
        "if [ \"$TYPE\" = \"wireguard\" ]; then\n"
        "  PORT=$(grep -oP 'ListenPort\\s*=\\s*\\K\\d+' \"$CFG\" | head -1)\n"
        "  [ -z \"$PORT\" ] && PORT=51820\n"
        "  if ss -lun 2>/dev/null | grep -q \":$PORT \";\n"
        "  then warn \"Porta UDP $PORT gia' in uso (server locale)\"; fi\n"
        "fi\n"
        "\n"
        "# 4. Estrai IP server e testa raggiungibilita'\n"
        "SRV=\"\"\n"
        "case \"$TYPE\" in\n"
        "  wireguard)  SRV=$(grep -oP 'Endpoint\\s*=\\s*\\K[^:\\s]+' \"$CFG\" | head -1) ;;\n"
        "  openvpn)    SRV=$(grep -oP '^remote\\s+\\K\\S+' \"$CFG\" | head -1) ;;\n"
        "  n2n_edge)   SRV=$(grep -oP 'SUPERNODE_IP=\"?\\K[^\"\\s]+' \"$CFG\" | head -1) ;;\n"
        "esac\n"
        "if [ -n \"$SRV\" ] && [[ \"$SRV\" != *'<'* ]]; then\n"
        "  warn \"Test ping $SRV (timeout 2s)...\"\n"
        "  if ping -c 1 -W 2 \"$SRV\" &>/dev/null 2>&1;\n"
        "  then ok \"Server raggiungibile: $SRV\";\n"
        "  else warn \"Ping non risponde: $SRV (potrebbe essere filtrato dal firewall)\"; fi\n"
        "fi\n"
        "\n"
        "# 5. Lunghezza minima config\n"
        "LINES=$(wc -l < \"$CFG\")\n"
        "if [ \"$LINES\" -lt 3 ]; then\n"
        "  warn \"Config molto corta ($LINES righe) — controlla che sia completa\"\n"
        "fi\n"
        "\n"
        "echo \"\"\n"
        "echo \"--- Validazione: $PASS OK  |  $FAIL errori  |  $WARN avvisi ---\"\n"
        "[ \"$FAIL\" -eq 0 ] && exit 0 || exit 1\n";

    const QString scriptPath = P::tmpDir() + "vpn_check.sh";
    QFile sf(scriptPath);
    if (!sf.open(QIODevice::WriteOnly | QIODevice::Text)) {
        m_vpnLog->append(
            "<span style='color:#f87171;'>\xe2\x9d\x8c  "
            "Impossibile creare lo script di validazione.</span>");
        return;
    }
    sf.write(script.toUtf8());
    sf.close();

    m_vpnValidating = true;
    if (m_vpnValidateBtn) m_vpnValidateBtn->setEnabled(false);
    m_vpnLog->append(
        "<span style='color:#94a3b8;'>\xf0\x9f\x94\x8d  "
        "Validazione in corso (senza root)...</span>");
    if (m_vpnStatusLbl) m_vpnStatusLbl->setText(tr("\xf0\x9f\x94\x8d  Validazione..."));
    LogBus::post(QString("\xf0\x9f\x94\x8d VPN — validazione avviata (%1)").arg(type));

    if (!m_vpnProc) {
        m_vpnProc = new QProcess(this);
        m_vpnProc->setProcessChannelMode(QProcess::MergedChannels);
        connect(m_vpnProc, &QProcess::readyRead,
                this, &ProgrammazionePage::onVpnProcReadyRead);
        connect(m_vpnProc,
                QOverload<int,QProcess::ExitStatus>::of(&QProcess::finished),
                this, &ProgrammazionePage::onVpnProcFinished);
        connect(m_vpnProc, &QProcess::errorOccurred,
                this, [this](QProcess::ProcessError err) {
            if (err == QProcess::FailedToStart)
                qWarning() << "[vpn_validate] processo non avviato:" << m_vpnProc->program();
        });
    }
    m_vpnProc->start(QStringLiteral("bash"), {scriptPath});
}

/* ── Importa config da file (DontUseNativeDialog evita crash Dolphin) ── */
void ProgrammazionePage::onVpnImportClicked()
{
    if (!m_vpnConfig || !m_vpnTypeCombo) return;

    static const char* kFilters[] = {
        "Config WireGuard (*.conf);;Tutti i file (*)",
        "Config OpenVPN (*.ovpn *.conf);;Tutti i file (*)",
        "Script shell (*.sh);;Tutti i file (*)",
        "Script shell (*.sh);;Tutti i file (*)",
        "Script shell (*.sh);;Tutti i file (*)",
        "Script shell (*.sh);;Tutti i file (*)",
    };
    const int idx = m_vpnTypeCombo->currentIndex();
    const QString filter = QString::fromUtf8(
        (idx >= 0 && idx < 6) ? kFilters[idx] : kFilters[2]);

    QFileDialog dlg(this, tr("Importa configurazione VPN"));
    dlg.setOption(QFileDialog::DontUseNativeDialog);   // evita crash Dolphin KDE
    dlg.setFileMode(QFileDialog::ExistingFile);
    dlg.setNameFilter(filter);

    if (dlg.exec() != QDialog::Accepted) return;
    const QStringList files = dlg.selectedFiles();
    if (files.isEmpty()) return;

    QFile f(files.first());
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) {
        if (m_vpnLog)
            m_vpnLog->append(
                "<span style='color:#f87171;'>\xe2\x9d\x8c  "
                "Impossibile aprire: " + files.first().toHtmlEscaped() + "</span>");
        return;
    }
    m_vpnConfig->setPlainText(QString::fromUtf8(f.readAll()));
    if (m_vpnLog)
        m_vpnLog->append(
            QString("<span style='color:#4ade80;'>\xf0\x9f\x93\x82  "
                    "Importato: %1</span>").arg(files.first().toHtmlEscaped()));
    LogBus::post("\xf0\x9f\x93\x82 VPN — config importata: " + files.first());
}

void ProgrammazionePage::onVpnProcReadyRead()
{
    if (!m_vpnProc || !m_vpnLog) return;
    const QString out = QString::fromUtf8(m_vpnProc->readAll()).trimmed();
    if (!out.isEmpty()) m_vpnLog->append(out.toHtmlEscaped());
}

void ProgrammazionePage::onVpnProcFinished(int code, QProcess::ExitStatus /*status*/)
{
    if (!m_vpnStatusLbl || !m_vpnLog) return;

    if (m_vpnValidating) {
        m_vpnValidating = false;
        if (m_vpnValidateBtn) m_vpnValidateBtn->setEnabled(true);
        if (code == 0) {
            m_vpnStatusLbl->setText(tr("\xe2\x9c\x85  Valida"));
            m_vpnLog->append(
                "<span style='color:#4ade80;'>\xe2\x9c\x85  "
                "<b>Configurazione valida</b> — pronta per essere applicata.</span>");
            LogBus::post("\xe2\x9c\x85 VPN — validazione superata");
        } else {
            m_vpnStatusLbl->setText(tr("\xe2\x9d\x8c  Errori trovati"));
            m_vpnLog->append(
                "<span style='color:#f87171;'>\xe2\x9d\x8c  "
                "<b>Trovati errori</b> — correggi prima di applicare.</span>");
            LogBus::post("\xe2\x9d\x8c VPN — validazione fallita");
        }
        return;
    }

    if (code == 0) {
        m_vpnStatusLbl->setText(tr("\xe2\x9c\x85  Completato"));
        m_vpnLog->append(
            "<span style='color:#4ade80;'>\xe2\x9c\x85  Comando completato correttamente.</span>");
    } else {
        m_vpnStatusLbl->setText(
            QString("\xe2\x9d\x8c  Uscito con codice %1").arg(code));
        m_vpnLog->append(
            QString("<span style='color:#f87171;'>\xe2\x9d\x8c  "
                    "Comando uscito con codice %1.</span>").arg(code));
    }
}


/* ══════════════════════════════════════════════════════════════
   buildVpnTab — sub-tab "🔒 VPN & Tunnel"
   Configura WireGuard, OpenVPN, SSH tunnel, Wi-Fi Hotspot con
   template pronti + generazione AI + applicazione via pkexec.
   ══════════════════════════════════════════════════════════════ */
QWidget* ProgrammazionePage::buildVpnTab(QWidget* parent)
{
    static const struct {
        const char* label;
        const char* desc;
        const char* tmpl;
    } kVpnTypes[] = {
        {
            "WireGuard",
            "\xf0\x9f\x94\x92 Protocollo VPN moderno — veloce, sicuro, semplice. "
            "Installa: <code>sudo apt install wireguard</code>",
            "# WireGuard — configurazione client\n"
            "# Salva come /etc/wireguard/wg0.conf e usa: sudo wg-quick up wg0\n"
            "\n"
            "[Interface]\n"
            "Address = 10.0.0.2/24\n"
            "PrivateKey = <CHIAVE_PRIVATA_CLIENT>\n"
            "DNS = 1.1.1.1\n"
            "\n"
            "[Peer]\n"
            "PublicKey = <CHIAVE_PUBBLICA_SERVER>\n"
            "AllowedIPs = 0.0.0.0/0, ::/0\n"
            "Endpoint = <SERVER_IP>:51820\n"
            "PersistentKeepalive = 25\n"
            "\n"
            "# Genera le chiavi con:\n"
            "#   wg genkey | tee privatekey | wg pubkey > publickey\n"
        },
        {
            "OpenVPN",
            "\xf0\x9f\x9b\xa1  VPN classica — alta compatibilit\xc3\xa0. "
            "Installa: <code>sudo apt install openvpn</code>",
            "# OpenVPN — configurazione client (.ovpn)\n"
            "# Usa: sudo openvpn --config client.ovpn\n"
            "\n"
            "client\n"
            "dev tun\n"
            "proto udp\n"
            "remote <SERVER_IP> 1194\n"
            "resolv-retry infinite\n"
            "nobind\n"
            "persist-key\n"
            "persist-tun\n"
            "cipher AES-256-CBC\n"
            "auth SHA256\n"
            "verb 3\n"
            "keepalive 10 120\n"
            "\n"
            "<ca>\n"
            "# Incolla il contenuto di ca.crt\n"
            "</ca>\n"
            "<cert>\n"
            "# Incolla il contenuto di client.crt\n"
            "</cert>\n"
            "<key>\n"
            "# Incolla il contenuto di client.key\n"
            "</key>\n"
        },
        {
            "SSH Tunnel",
            "\xf0\x9f\x94\x8c Reindirizzamento porte SSH — senza software extra.",
            "# SSH Tunnel — esempi pronti all'uso\n"
            "\n"
            "# 1. Tunnel LOCALE: porta locale 8080 -> server:80\n"
            "ssh -N -L 8080:localhost:80 utente@<SERVER>\n"
            "\n"
            "# 2. Tunnel REMOTO: porta remota 9090 -> localhost:80\n"
            "ssh -N -R 9090:localhost:80 utente@<SERVER>\n"
            "\n"
            "# 3. SOCKS proxy dinamico (porta 1080)\n"
            "ssh -N -D 1080 -C utente@<SERVER>\n"
            "\n"
            "# 4. Sessione persistente con autossh\n"
            "#    sudo apt install autossh\n"
            "autossh -M 20000 -N -L 8080:localhost:80 utente@<SERVER>\n"
            "\n"
            "# Configura in /etc/ssh/sshd_config del server:\n"
            "#   AllowTcpForwarding yes\n"
            "#   GatewayPorts yes       # solo per tunnel remoti\n"
        },
        {
            "Wi-Fi Hotspot",
            "\xf0\x9f\x93\xa1 Condividi la connessione via Wi-Fi con nmcli.",
            "#!/bin/bash\n"
            "# Wi-Fi Hotspot con NetworkManager\n"
            "# Adatta SSID, password e interfaccia (wlan0 / wlp3s0)\n"
            "\n"
            "IFACE=\"wlan0\"\n"
            "SSID=\"PrismaluxNet\"\n"
            "PASSWORD=\"password_sicura\"\n"
            "CON_NAME=\"PrismaluxAP\"\n"
            "\n"
            "# Crea la connessione hotspot\n"
            "nmcli con add type wifi ifname \"$IFACE\" con-name \"$CON_NAME\" \\\n"
            "  ssid \"$SSID\" mode ap\n"
            "\n"
            "# Configura sicurezza e canale\n"
            "nmcli con modify \"$CON_NAME\" \\\n"
            "  wifi.band bg wifi.channel 6 \\\n"
            "  wifi-sec.key-mgmt wpa-psk wifi-sec.psk \"$PASSWORD\"\n"
            "\n"
            "# Condivisione connessione (NAT)\n"
            "nmcli con modify \"$CON_NAME\" ipv4.method shared\n"
            "\n"
            "# Avvia\n"
            "nmcli con up \"$CON_NAME\"\n"
            "echo \"Hotspot '$SSID' attivo su $IFACE\"\n"
            "\n"
            "# Per fermare:\n"
            "# nmcli con down \"$CON_NAME\"\n"
        },
        {
            "n2n Supernode",
            "\xf0\x9f\x8c\x90 Nodo centrale VPN n2n (C puro) per WAN Prismalux. "
            "Installa: <code>sudo apt install n2n</code>. "
            "Apri firewall: <code>sudo ufw allow 7654/udp</code>.",
            "#!/bin/bash\n"
            "# n2n Supernode — nodo centrale WAN Prismalux (scritto in C)\n"
            "SUPERNODE_PORT=7654\n"
            "supernode -p \"$SUPERNODE_PORT\" -v\n"
        },
        {
            "n2n Edge (worker)",
            "\xf0\x9f\x94\x97 Worker WAN n2n — cifrato AES-256, scritto in C. "
            "Clicca <b>Genera chiavi n2n</b>, poi <b>Applica</b> per copiare "
            "lo script da avviare su ogni nodo remoto con <code>sudo bash</code>.",
            "#!/bin/bash\n"
            "# n2n Edge — worker WAN Prismalux\n"
            "SUPERNODE_IP=\"<SERVER_IP>\"\n"
            "COMMUNITY=\"prismalux_wan\"\n"
            "PSK=\"<CHIAVE_CONDIVISA_AES256>\"\n"
            "EDGE_IP=\"10.10.0.2/24\"\n"
            "edge -c \"$COMMUNITY\" -k \"$PSK\" -a \"$EDGE_IP\" \\\n"
            "     -l \"$SUPERNODE_IP:7654\" -f\n"
        },
    };
    constexpr int kN = 6;

    auto* w   = new QWidget(parent);
    auto* lay = new QVBoxLayout(w);
    lay->setContentsMargins(12, 12, 12, 12);
    lay->setSpacing(8);

    /* ── Riga tipo ── */
    auto* typeRow = new QWidget(w);
    auto* typeHL  = new QHBoxLayout(typeRow);
    typeHL->setContentsMargins(0, 0, 0, 0);
    typeHL->setSpacing(8);

    auto* typeLbl = new QLabel("\xf0\x9f\x94\x92  Tipo:", typeRow);
    m_vpnTypeCombo = new QComboBox(typeRow);
    m_vpnTypeCombo->setFixedWidth(dpiScale(160));
    for (int i = 0; i < kN; ++i)
        m_vpnTypeCombo->addItem(QString::fromUtf8(kVpnTypes[i].label));

    m_vpnStatusLbl = new QLabel(typeRow);
    m_vpnStatusLbl->setObjectName("hintLabel");
    m_vpnStatusLbl->setWordWrap(false);

    typeHL->addWidget(typeLbl);
    typeHL->addWidget(m_vpnTypeCombo);
    typeHL->addStretch();
    typeHL->addWidget(m_vpnStatusLbl);
    lay->addWidget(typeRow);

    /* ── Descrizione tipo ── */
    auto* descLbl = new QLabel(w);
    descLbl->setObjectName("hintLabel");
    descLbl->setTextFormat(Qt::RichText);
    descLbl->setWordWrap(true);
    descLbl->setText(QString::fromUtf8(kVpnTypes[0].desc));
    lay->addWidget(descLbl);

    /* ── Nota: quando serve la VPN ── */
    auto* vpnHintLbl = new QLabel(w);
    vpnHintLbl->setObjectName("hintLabel");
    vpnHintLbl->setTextFormat(Qt::RichText);
    vpnHintLbl->setWordWrap(true);
    vpnHintLbl->setText(
        "<b>" "\xf0\x9f\x92\xa1" " Quando serve la VPN:</b> "
        "Se i nodi WAN Compute o Sci Compute si trovano su reti diverse "
        "(internet, ufficio remoto, casa), la VPN crea un tunnel cifrato "
        "che li fa sembrare sulla stessa LAN. "
        "In questo modo il WAN Compute (porta 11600) e il Sci Compute (porta 11601) "
        "restano raggiungibili e sicuri senza aprire porte sul router. "
        "Per nodi sulla stessa LAN la VPN non \xc3\xa8 necessaria.");
    lay->addWidget(vpnHintLbl);

    /* ── Editor config / script ── */
    auto* cfgGroup = new QGroupBox(
        "\xf0\x9f\x93\x9d  Configurazione (modificabile)", w);
    auto* cfgLay   = new QVBoxLayout(cfgGroup);
    m_vpnConfig = new QTextEdit(cfgGroup);
    m_vpnConfig->setFont(QFont("JetBrains Mono,Fira Code,Consolas,Monospace", 9));
    m_vpnConfig->setMinimumHeight(dpiScale(180));
    m_vpnConfig->setPlaceholderText(tr("Template qui..."));
    m_vpnConfig->setPlainText(QString::fromUtf8(kVpnTypes[0].tmpl));
    cfgLay->addWidget(m_vpnConfig);
    lay->addWidget(cfgGroup, 1);

    /* ── Barra azioni ── */
    auto* actRow = new QWidget(w);
    auto* actHL  = new QHBoxLayout(actRow);
    actHL->setContentsMargins(0, 0, 0, 0);
    actHL->setSpacing(8);

    auto* btnGen   = new QPushButton(
        "\xf0\x9f\xa4\x96  Migliora con AI", actRow);
    btnGen->setObjectName("actionBtn");
    auto* btnApply = new QPushButton(
        "\xe2\x9a\xa1  Applica / Esegui", actRow);
    btnApply->setObjectName("actionBtn");
    auto* btnCopy  = new QPushButton(
        "\xf0\x9f\x93\x8b  Copia", actRow);
    auto* btnStop  = new QPushButton(
        "\xe2\x8f\xb9  Stop", actRow);
    btnStop->setProperty("danger", true);
    btnStop->setEnabled(false);

    m_vpnGenKeysBtn = new QPushButton(
        "\xf0\x9f\x94\x91  Genera chiavi n2n", actRow);
    m_vpnGenKeysBtn->setToolTip(tr("Genera community name e PSK casuali per n2n"));
    m_vpnGenKeysBtn->setVisible(false);

    m_vpnValidateBtn = new QPushButton(
        "\xf0\x9f\x94\x8d  Valida config", actRow);
    m_vpnValidateBtn->setToolTip(
        tr("Simula e valida la configurazione senza avviare la VPN\n"
           "Controlla: placeholder, binari, porte, raggiungibilita' server"));

    auto* btnImport = new QPushButton(
        "\xf0\x9f\x93\x82  Importa", actRow);
    btnImport->setToolTip(tr("Carica un file di configurazione VPN esistente"));

    actHL->addWidget(btnGen);
    actHL->addWidget(m_vpnGenKeysBtn);
    actHL->addStretch();
    actHL->addWidget(btnImport);
    actHL->addWidget(m_vpnValidateBtn);
    actHL->addWidget(btnCopy);
    actHL->addWidget(btnApply);
    actHL->addWidget(btnStop);
    lay->addWidget(actRow);

    /* ── Stato connessione VPN live ── */
    auto* statusRow = new QWidget(w);
    auto* statusHL  = new QHBoxLayout(statusRow);
    statusHL->setContentsMargins(0, 0, 0, 0);
    auto* statusCap = new QLabel(tr("Connessione:"), statusRow);
    m_vpnLiveStatusLbl = new QLabel(tr("\xe2\x9a\xaa  Nessuna VPN attiva"), statusRow);
    m_vpnTestBtn = new QPushButton("\xf0\x9f\x94\x8d  Verifica stato", statusRow);
    m_vpnTestBtn->setToolTip(tr("Rileva interfacce VPN attive (wg/tun) e il loro IP, senza root"));
    statusHL->addWidget(statusCap);
    statusHL->addWidget(m_vpnLiveStatusLbl, 1);
    statusHL->addWidget(m_vpnTestBtn);
    lay->addWidget(statusRow);

    /* ── Log output ── */
    auto* logGroup = new QGroupBox(
        "\xf0\x9f\x93\x9f  Output comandi", w);
    auto* logLay   = new QVBoxLayout(logGroup);
    m_vpnLog = new QTextEdit(logGroup);
    m_vpnLog->setReadOnly(true);
    m_vpnLog->setFont(QFont("JetBrains Mono,Fira Code,Consolas,Monospace", 9));
    m_vpnLog->setMinimumHeight(dpiScale(100));
    logLay->addWidget(m_vpnLog);
    lay->addWidget(logGroup);

    /* ── Connessioni ── */
    connect(m_vpnTypeCombo,
            QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &ProgrammazionePage::onVpnTypeChanged);

    /* Aggiorna desc al cambio tipo */
    connect(m_vpnTypeCombo,
            QOverload<int>::of(&QComboBox::currentIndexChanged),
            descLbl,
            [descLbl](int idx) {
        static const char* descs[] = {
            "\xf0\x9f\x94\x92 Protocollo VPN moderno — veloce, sicuro, semplice. "
            "Installa: <code>sudo apt install wireguard</code>",
            "\xf0\x9f\x9b\xa1  VPN classica — alta compatibilit\xc3\xa0. "
            "Installa: <code>sudo apt install openvpn</code>",
            "\xf0\x9f\x94\x8c Reindirizzamento porte SSH — senza software extra.",
            "\xf0\x9f\x93\xa1 Condividi la connessione via Wi-Fi con nmcli.",
            "\xf0\x9f\x8c\x90 Nodo centrale VPN n2n (C puro) per WAN Prismalux. "
            "Installa: <code>sudo apt install n2n</code>. "
            "Apri firewall: <code>sudo ufw allow 7654/udp</code>.",
            "\xf0\x9f\x94\x97 Worker WAN n2n — cifrato AES-256, scritto in C. "
            "Clicca <b>Genera chiavi n2n</b>, poi <b>Applica</b> per copiare "
            "lo script da avviare su ogni nodo remoto con <code>sudo bash</code>.",
        };
        if (idx >= 0 && idx < 6)
            descLbl->setText(QString::fromUtf8(descs[idx]));
    });

    connect(btnCopy, &QPushButton::clicked, m_vpnConfig,
            [this, btnCopy]() {
        qApp->clipboard()->setText(m_vpnConfig->toPlainText());
        const QString orig = btnCopy->text();
        btnCopy->setText(tr("\xe2\x9c\x85  Copiato!"));
        QTimer::singleShot(1500, btnCopy, [btnCopy, orig]() {
            btnCopy->setText(orig);
        });
    });

    connect(btnGen, &QPushButton::clicked, this,
            [this, btnGen, btnStop]() {
        btnGen->setEnabled(false);
        btnStop->setEnabled(true);
        onVpnGenerateClicked();
    });

    connect(btnApply, &QPushButton::clicked,
            this, &ProgrammazionePage::onVpnApplyClicked);

    connect(btnStop, &QPushButton::clicked, this,
            [this, btnGen, btnStop]() {
        m_ai->abort();
        if (m_vpnProc && m_vpnProc->state() != QProcess::NotRunning)
            m_vpnProc->terminate();
        btnGen->setEnabled(true);
        btnStop->setEnabled(false);
        if (m_vpnStatusLbl) m_vpnStatusLbl->setText(tr("\xe2\x8f\xb9  Fermato"));
    });

    connect(m_vpnGenKeysBtn, &QPushButton::clicked,
            this, &ProgrammazionePage::onVpnGenN2nKeys);

    connect(m_vpnValidateBtn, &QPushButton::clicked,
            this, &ProgrammazionePage::onVpnValidateClicked);

    connect(btnImport, &QPushButton::clicked,
            this, &ProgrammazionePage::onVpnImportClicked);

    connect(m_vpnTestBtn, &QPushButton::clicked,
            this, &ProgrammazionePage::onVpnTestClicked);

    /* Polling leggero dello stato VPN (QNetworkInterface, nessun processo). */
    if (!m_vpnStatusTimer) {
        m_vpnStatusTimer = new QTimer(this);
        m_vpnStatusTimer->setInterval(5000);
        connect(m_vpnStatusTimer, &QTimer::timeout,
                this, &ProgrammazionePage::vpnRefreshStatus);
        m_vpnStatusTimer->start();
    }
    vpnRefreshStatus();

    return w;
}
