/* ======================================================================
   main_net_policy.cpp — Generatore di Policy di rete/sicurezza con AI

   Sub-tab "🛡️ Policy" di Rete & Network in ProgrammazionePage.
   Supporta: IPsec (strongSwan), iptables, nftables, UFW, WireGuard ACL,
             n2n community policy.
   ====================================================================== */
#include "main_programming.h"
#include "../log_bus.h"
#include "../dpi_utils.h"
#include "../prismalux_paths.h"

#include <QApplication>
#include <QClipboard>
#include <QComboBox>
#include <QGroupBox>
#include <QLabel>
#include <QPushButton>
#include <QTextEdit>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QTimer>
#include <QSettings>

namespace P = PrismaluxPaths;

/* ──────────────────────────────────────────────────────────────────────
   Dati statici per ogni tipo di policy
   ────────────────────────────────────────────────────────────────────── */
namespace {

struct PolicyType {
    const char* label;
    const char* hint;
    const char* example;
    const char* systemPrompt;
};

static const PolicyType kTypes[] = {
    /* 0 — IPsec / strongSwan */
    {
        "IPsec / strongSwan",
        "\xf0\x9f\x94\x90 Genera <b>/etc/ipsec.conf</b> + <b>/etc/ipsec.secrets</b> "
        "per strongSwan IKEv2 site-to-site o road-warrior.",
        "Connessione IKEv2 site-to-site tra 10.0.1.0/24 e 10.0.2.0/24. "
        "Server A: 1.2.3.4 — Server B: 5.6.7.8. Autenticazione con PSK.",
        "Sei un esperto di strongSwan IKEv2 su Linux. "
        "Genera ESATTAMENTE due file separati con questa struttura:\n\n"
        "### /etc/ipsec.conf ###\n"
        "```\n"
        "# ipsec.conf - strongSwan IKEv2 configuration\n"
        "config setup\n"
        "    charondebug=\"ike 1, knl 1, cfg 0\"\n"
        "    uniqueids=no\n\n"
        "conn <nome-tunnel>\n"
        "    keyexchange=ikev2\n"
        "    left=<IP_SERVER_A>\n"
        "    leftsubnet=<CIDR_A>\n"
        "    leftid=@serverA\n"
        "    right=<IP_SERVER_B>\n"
        "    rightsubnet=<CIDR_B>\n"
        "    rightid=@serverB\n"
        "    authby=secret\n"
        "    auto=start\n"
        "    ike=aes256-sha256-modp2048!\n"
        "    esp=aes256-sha256!\n"
        "    ikelifetime=1h\n"
        "    lifetime=30m\n"
        "    dpdaction=restart\n"
        "    dpddelay=30s\n"
        "```\n\n"
        "### /etc/ipsec.secrets ###\n"
        "```\n"
        "# ipsec.secrets\n"
        "<IP_A> <IP_B> : PSK \"<chiave-segreta>\"\n"
        "```\n\n"
        "### Comandi utili ###\n"
        "```bash\n"
        "# Avvio e stato\n"
        "systemctl start strongswan\n"
        "ipsec status\n"
        "ipsec statusall\n"
        "```\n\n"
        "Usa SOLO questa sintassi strongSwan nativa. "
        "NON inventare keyword inesistenti. "
        "Adatta IP, subnet e PSK alla descrizione dell'utente. "
        "Rispondi SOLO con i file e i comandi, senza spiegazioni aggiuntive."
    },
    /* 1 — iptables */
    {
        "iptables",
        "\xf0\x9f\x9b\xa1 Script bash con <b>iptables</b> per Linux. "
        "Policy DROP di default, regole esplicite per INPUT/OUTPUT/FORWARD.",
        "Permetti SSH (22), HTTP (80), HTTPS (443) in ingresso. "
        "Blocca tutto il resto. Consenti traffico WireGuard UDP 51820. "
        "Log dei pacchetti bloccati.",
        "Sei un esperto di iptables su Linux. "
        "Genera uno script bash ESEGUIBILE con questa struttura ESATTA:\n\n"
        "```bash\n"
        "#!/bin/bash\n"
        "# Firewall iptables — generato da Prismalux\n"
        "set -e\n\n"
        "# === RESET COMPLETO ===\n"
        "iptables -F\n"
        "iptables -X\n"
        "iptables -Z\n"
        "ip6tables -F 2>/dev/null || true\n\n"
        "# === POLICY DI DEFAULT ===\n"
        "iptables -P INPUT   DROP\n"
        "iptables -P FORWARD DROP\n"
        "iptables -P OUTPUT  ACCEPT\n\n"
        "# === REGOLE BASE ===\n"
        "iptables -A INPUT -i lo -j ACCEPT\n"
        "iptables -A INPUT -m conntrack --ctstate ESTABLISHED,RELATED -j ACCEPT\n\n"
        "# === REGOLE SPECIFICHE ===\n"
        "# (qui vanno le regole richieste dall'utente)\n\n"
        "# === LOG PACCHETTI BLOCCATI ===\n"
        "iptables -A INPUT -j LOG --log-prefix \"[IPTABLES DROP] \" --log-level 4\n\n"
        "# === SALVA LE REGOLE ===\n"
        "if command -v iptables-save &>/dev/null; then\n"
        "    iptables-save > /etc/iptables/rules.v4\n"
        "    echo 'Regole salvate in /etc/iptables/rules.v4'\n"
        "fi\n"
        "```\n\n"
        "Sostituisci le regole specifiche con quelle richieste dall'utente. "
        "USA SOLO comandi iptables reali. "
        "Rispondi SOLO con lo script bash, senza spiegazioni aggiuntive."
    },
    /* 2 — nftables */
    {
        "nftables",
        "\xf0\x9f\x94\xa5 File <b>/etc/nftables.conf</b> — sostituto moderno di iptables. "
        "Sintassi nft compatta e performante.",
        "Firewall per server VPN: accetta traffico n2n UDP 7654, "
        "WireGuard UDP 51820, SSH 22. Blocca il resto in input.",
        "Sei un esperto di nftables su Linux. "
        "Genera ESATTAMENTE il file /etc/nftables.conf con questa struttura:\n\n"
        "```\n"
        "#!/usr/sbin/nft -f\n"
        "# nftables.conf — generato da Prismalux\n\n"
        "flush ruleset\n\n"
        "table inet filter {\n"
        "    chain input {\n"
        "        type filter hook input priority 0; policy drop;\n\n"
        "        # Connessioni esistenti\n"
        "        ct state established,related accept\n"
        "        # Loopback\n"
        "        iif lo accept\n"
        "        # Ping ICMP\n"
        "        ip protocol icmp accept\n"
        "        ip6 nexthdr icmpv6 accept\n\n"
        "        # REGOLE SPECIFICHE QUI\n\n"
        "        # Log e drop del resto\n"
        "        log prefix \"[NFT DROP] \" level warn\n"
        "        drop\n"
        "    }\n\n"
        "    chain forward {\n"
        "        type filter hook forward priority 0; policy drop;\n"
        "    }\n\n"
        "    chain output {\n"
        "        type filter hook output priority 0; policy accept;\n"
        "    }\n"
        "}\n"
        "```\n\n"
        "Comandi:\n"
        "```bash\n"
        "nft -f /etc/nftables.conf   # applica\n"
        "nft list ruleset             # verifica\n"
        "systemctl enable nftables    # avvio automatico\n"
        "```\n\n"
        "USA SOLO sintassi nft reale (nft 0.9+). "
        "Rispondi SOLO con il file di configurazione e i comandi, senza spiegazioni."
    },
    /* 3 — UFW */
    {
        "UFW (Ubuntu)",
        "\xf0\x9f\x94\xb5 Comandi <b>ufw</b> pronti all'uso per Ubuntu/Debian. "
        "Script bash applicabile direttamente.",
        "Abilita UFW. Permetti SSH, HTTP, HTTPS. "
        "Abilita porta 11600/tcp per WAN Prismalux. Blocca ping ICMP.",
        "Sei un esperto di UFW (Uncomplicated Firewall) su Ubuntu/Debian. "
        "Genera uno script bash ESEGUIBILE con questa struttura:\n\n"
        "```bash\n"
        "#!/bin/bash\n"
        "# UFW firewall — generato da Prismalux\n"
        "set -e\n\n"
        "# Reset e disabilita\n"
        "ufw --force reset\n"
        "ufw default deny incoming\n"
        "ufw default allow outgoing\n\n"
        "# REGOLE SPECIFICHE QUI\n"
        "# Esempio:\n"
        "# ufw allow 22/tcp          # SSH\n"
        "# ufw allow 80/tcp          # HTTP\n"
        "# ufw allow from 192.168.1.0/24 to any port 8080\n"
        "# ufw deny 23/tcp           # Telnet\n\n"
        "# Abilita\n"
        "ufw --force enable\n"
        "ufw status verbose\n"
        "```\n\n"
        "Per ICMP usa: ufw deny proto icmp\n"
        "Per rate limiting: ufw limit <porta>/tcp\n"
        "USA SOLO comandi ufw reali. "
        "Rispondi SOLO con lo script bash, senza spiegazioni aggiuntive."
    },
    /* 4 — WireGuard ACL */
    {
        "WireGuard ACL",
        "\xf0\x9f\x94\x97 <b>AllowedIPs</b> per-peer in WireGuard — "
        "controlla quale traffico instrada tra i peer.",
        "Peer A (10.0.0.2): accede solo a 10.0.0.1 e 192.168.1.0/24. "
        "Peer B (10.0.0.3): accesso completo 0.0.0.0/0. "
        "Peer C (10.0.0.4): solo porta 11600 verso 10.0.0.1.",
        "Sei un esperto di WireGuard su Linux. "
        "Genera i file di configurazione WireGuard con questa struttura ESATTA:\n\n"
        "### /etc/wireguard/wg0.conf (SERVER) ###\n"
        "```ini\n"
        "[Interface]\n"
        "Address = 10.0.0.1/24\n"
        "ListenPort = 51820\n"
        "PrivateKey = <CHIAVE_PRIVATA_SERVER>  # genera con: wg genkey\n"
        "PostUp   = iptables -A FORWARD -i wg0 -j ACCEPT; iptables -t nat -A POSTROUTING -o eth0 -j MASQUERADE\n"
        "PostDown = iptables -D FORWARD -i wg0 -j ACCEPT; iptables -t nat -D POSTROUTING -o eth0 -j MASQUERADE\n\n"
        "# Peer A\n"
        "[Peer]\n"
        "PublicKey = <CHIAVE_PUBBLICA_PEER_A>  # genera con: wg pubkey < privkey\n"
        "AllowedIPs = 10.0.0.2/32  # solo il peer, nessun forwarding\n\n"
        "# Peer B\n"
        "[Peer]\n"
        "PublicKey = <CHIAVE_PUBBLICA_PEER_B>\n"
        "AllowedIPs = 10.0.0.3/32\n"
        "```\n\n"
        "### /etc/wireguard/wg0.conf (CLIENT) ###\n"
        "```ini\n"
        "[Interface]\n"
        "Address = 10.0.0.X/32\n"
        "PrivateKey = <CHIAVE_PRIVATA_CLIENT>\n"
        "DNS = 1.1.1.1\n\n"
        "[Peer]\n"
        "PublicKey = <CHIAVE_PUBBLICA_SERVER>\n"
        "Endpoint = <IP_SERVER>:51820\n"
        "AllowedIPs = 0.0.0.0/0  # o subnet specifica per split tunnel\n"
        "PersistentKeepalive = 25\n"
        "```\n\n"
        "Comandi:\n"
        "```bash\n"
        "wg-quick up wg0\n"
        "wg show\n"
        "systemctl enable wg-quick@wg0\n"
        "```\n\n"
        "NOTE CRITICHE: AllowedIPs controlla il ROUTING, non un firewall applicativo. "
        "Per ACL per-porta combina con PostUp iptables. "
        "Adatta IP e AllowedIPs alla descrizione dell'utente. "
        "Rispondi SOLO con i file di configurazione e i comandi."
    },
    /* 5 — n2n Community Policy */
    {
        "n2n Community Policy",
        "\xf0\x9f\x8c\x90 Script bash per gestire community, chiavi AES e firewall "
        "n2n sul supernode.",
        "Crea due community separate: 'prismalux_prod' e 'prismalux_dev'. "
        "Limita 'prod' a max 10 edge. Aggiungi regola iptables per isolare le due community.",
        "Sei un esperto di n2n (edge networking) versione 3.x su Linux. "
        "Genera uno script bash ESEGUIBILE che configura supernode e edge n2n:\n\n"
        "```bash\n"
        "#!/bin/bash\n"
        "# n2n community policy — generato da Prismalux\n"
        "set -e\n\n"
        "# === SUPERNODE ===\n"
        "# File: /etc/n2n/supernode.conf\n"
        "cat > /etc/n2n/supernode.conf << 'EOF'\n"
        "# n2n supernode configuration\n"
        "-p=7654           # porta di ascolto UDP\n"
        "-t=7545           # porta management\n"
        "-f                # foreground (rimuovi per daemon)\n"
        "EOF\n\n"
        "# Avvia supernode\n"
        "# supernode -c /etc/n2n/supernode.conf\n\n"
        "# === EDGE NODE ===\n"
        "# edge -c <community> -k <PSK-AES256> -a <IP> -l <supernode>:7654\n\n"
        "# === FIREWALL ISOLAMENTO COMMUNITY ===\n"
        "# Le community n2n condividono la stessa porta UDP: l'isolamento\n"
        "# avviene tramite cifratura AES256 con chiavi diverse per community.\n"
        "# Aggiungi qui le regole iptables per il traffico edge:\n\n"
        "# Esempio: blocca traffico tra le subnet di community diverse\n"
        "# iptables -A FORWARD -s 10.0.1.0/24 -d 10.0.2.0/24 -j DROP\n"
        "```\n\n"
        "USA SOLO opzioni n2n 3.x reali (edge, supernode). "
        "Rispondi SOLO con lo script bash, senza spiegazioni aggiuntive."
    },
};
constexpr int kN = 6;

} // namespace

/* ──────────────────────────────────────────────────────────────────────
   buildPolicyTab — costruisce il sub-tab Policy Generator
   ────────────────────────────────────────────────────────────────────── */
QWidget* ProgrammazionePage::buildPolicyTab(QWidget* parent)
{
    auto* w   = new QWidget(parent);
    auto* lay = new QVBoxLayout(w);
    lay->setContentsMargins(12, 12, 12, 12);
    lay->setSpacing(8);

    /* ── Riga tipo + modello ── */
    auto* topRow = new QWidget(w);
    auto* topHL  = new QHBoxLayout(topRow);
    topHL->setContentsMargins(0, 0, 0, 0);
    topHL->setSpacing(8);

    topHL->addWidget(new QLabel(tr("\xf0\x9f\x9b\xa1  Tipo:"), topRow));
    m_policyTypeCombo = new QComboBox(topRow);
    m_policyTypeCombo->setMinimumWidth(dpiScale(190));
    for (int i = 0; i < kN; ++i)
        m_policyTypeCombo->addItem(QString::fromUtf8(kTypes[i].label));
    topHL->addWidget(m_policyTypeCombo);

    topHL->addSpacing(12);

    topHL->addWidget(new QLabel(tr("\xf0\x9f\xa7\xa0  Modello:"), topRow));
    m_policyModelCombo = new QComboBox(topRow);
    m_policyModelCombo->setMinimumWidth(dpiScale(200));
    m_policyModelCombo->setToolTip(
        "Scegli un modello dedicato alla generazione di codice/policy.\n"
        "Consigliati: deepseek-coder, qwen2.5-coder, codellama, mistral,\n"
        "o modelli reasoning come deepseek-r1, qwen3, qwq.");
    topHL->addWidget(m_policyModelCombo, 1);

    auto* btnRefreshModels = new QPushButton("\xf0\x9f\x94\x84", topRow);
    btnRefreshModels->setFixedWidth(dpiScale(30));
    btnRefreshModels->setToolTip(tr("Ricarica lista modelli da Ollama"));
    topHL->addWidget(btnRefreshModels);

    m_policyStatusLbl = new QLabel(topRow);
    m_policyStatusLbl->setObjectName("hintLabel");
    m_policyStatusLbl->setWordWrap(false);
    topHL->addWidget(m_policyStatusLbl);

    lay->addWidget(topRow);

    /* ── Descrizione tipo ── */
    auto* hintLbl = new QLabel(w);
    hintLbl->setObjectName("hintLabel");
    hintLbl->setTextFormat(Qt::RichText);
    hintLbl->setWordWrap(true);
    hintLbl->setText(QString::fromUtf8(kTypes[0].hint));
    lay->addWidget(hintLbl);

    /* ── Input descrizione ── */
    auto* descGroup = new QGroupBox(
        "\xf0\x9f\x93\x9d  Descrivi la policy (in linguaggio naturale)", w);
    auto* descLay = new QVBoxLayout(descGroup);
    m_policyDesc = new QTextEdit(descGroup);
    m_policyDesc->setPlaceholderText(
        tr("Es: Permetti SSH e HTTPS in entrata, blocca tutto il resto..."));
    m_policyDesc->setPlainText(QString::fromUtf8(kTypes[0].example));
    m_policyDesc->setFixedHeight(dpiScale(90));
    m_policyDesc->setFont(QFont("JetBrains Mono,Fira Code,Consolas,Monospace", 9));
    descLay->addWidget(m_policyDesc);
    lay->addWidget(descGroup);

    /* ── Output policy ── */
    auto* outGroup = new QGroupBox(
        "\xf0\x9f\x93\x84  Policy generata (modificabile — pronta per copia/incolla)", w);
    auto* outLay = new QVBoxLayout(outGroup);
    m_policyOutput = new QTextEdit(outGroup);
    m_policyOutput->setFont(QFont("JetBrains Mono,Fira Code,Consolas,Monospace", 9));
    m_policyOutput->setMinimumHeight(dpiScale(200));
    m_policyOutput->setPlaceholderText(
        tr("La policy generata dall'AI apparir\xc3\xa0 qui..."));
    outLay->addWidget(m_policyOutput);
    lay->addWidget(outGroup, 1);

    /* ── Barra azioni ── */
    auto* actRow = new QWidget(w);
    auto* actHL  = new QHBoxLayout(actRow);
    actHL->setContentsMargins(0, 0, 0, 0);
    actHL->setSpacing(8);

    auto* btnGen  = new QPushButton(tr("\xf0\x9f\xa4\x96  Genera policy"), actRow);
    btnGen->setObjectName("actionBtn");
    auto* btnCopy = new QPushButton(tr("\xf0\x9f\x93\x8b  Copia"), actRow);
    auto* btnStop = new QPushButton(tr("\xe2\x8f\xb9  Stop"), actRow);
    btnStop->setProperty("danger", true);
    btnStop->setEnabled(false);

    actHL->addWidget(btnGen);
    actHL->addStretch();
    actHL->addWidget(btnCopy);
    actHL->addWidget(btnStop);
    lay->addWidget(actRow);

    /* ── Connessioni ── */

    connect(m_policyTypeCombo,
            QOverload<int>::of(&QComboBox::currentIndexChanged),
            w, [this, hintLbl](int idx) {
        if (idx < 0 || idx >= kN) return;
        hintLbl->setText(QString::fromUtf8(kTypes[idx].hint));
        m_policyDesc->setPlainText(QString::fromUtf8(kTypes[idx].example));
    });

    connect(btnRefreshModels, &QPushButton::clicked,
            this, &ProgrammazionePage::onPolicyRefreshModels);

    connect(btnCopy, &QPushButton::clicked, m_policyOutput,
            [this, btnCopy]() {
        qApp->clipboard()->setText(m_policyOutput->toPlainText());
        const QString orig = btnCopy->text();
        btnCopy->setText(tr("\xe2\x9c\x85  Copiato!"));
        QTimer::singleShot(1500, btnCopy, [btnCopy, orig]{ btnCopy->setText(orig); });
    });

    connect(btnGen, &QPushButton::clicked, this,
            [this, btnGen, btnStop]() {
        btnGen->setEnabled(false);
        btnStop->setEnabled(true);
        onPolicyGenerateClicked();
    });

    connect(btnStop, &QPushButton::clicked, this,
            [this, btnGen, btnStop]() {
        m_ai->abort();
        if (!m_policyPrevModel.isEmpty()) {
            m_ai->setModel(m_policyPrevModel);
            m_policyPrevModel.clear();
        }
        disconnect(m_policyAiTokConn);
        disconnect(m_policyAiFinConn);
        disconnect(m_policyAiErrConn);
        btnGen->setEnabled(true);
        btnStop->setEnabled(false);
        if (m_policyStatusLbl) m_policyStatusLbl->setText(tr("\xe2\x8f\xb9  Fermato"));
    });

    /* Carica modelli al primo show */
    QTimer::singleShot(0, this, &ProgrammazionePage::onPolicyRefreshModels);

    return w;
}

/* ──────────────────────────────────────────────────────────────────────
   onPolicyRefreshModels — ricarica la lista modelli da Ollama
   ────────────────────────────────────────────────────────────────────── */
void ProgrammazionePage::onPolicyRefreshModels()
{
    if (!m_policyModelCombo || !m_ai) return;

    const QString prev = m_policyModelCombo->currentData(Qt::UserRole).toString();
    m_policyModelCombo->clear();
    m_policyModelCombo->addItem("\xe2\x8f\xb3  Caricamento...", "");

    auto* holder = new QObject(this);
    connect(m_ai, &AiClient::modelsReady, holder,
            [this, holder, prev](const QStringList& list) {
        holder->deleteLater();
        if (!m_policyModelCombo) return;

        m_policyModelCombo->clear();

        /* Aggiungi prima il modello globale corrente come opzione */
        const QString globalModel = m_ai->model();
        m_policyModelCombo->addItem(
            P::modelIcon(0, globalModel) +
            globalModel + " (modello attivo)",
            globalModel);

        /* Tag per ordinare: coding/reasoning in cima */
        auto rank = [](const QString& n) -> int {
            const QString l = n.toLower();
            if (l.contains("coder") || l.contains("code"))  return 0;
            if (l.contains("r1")   || l.contains("qwq") ||
                l.contains("reasoning") || l.contains("think")) return 1;
            if (l.contains("qwen") || l.contains("deepseek"))   return 2;
            if (l.contains("mistral") || l.contains("llama"))   return 3;
            return 4;
        };

        QStringList sorted = list;
        std::stable_sort(sorted.begin(), sorted.end(),
            [&rank](const QString& a, const QString& b){
                return rank(a) < rank(b);
            });

        for (const QString& m : sorted) {
            if (m == globalModel) continue;  /* già aggiunto come primo */
            m_policyModelCombo->addItem(P::modelIcon(0, m) + m, m);
        }

        /* Ripristina selezione precedente */
        const int idx = m_policyModelCombo->findData(prev);
        if (idx >= 0) m_policyModelCombo->setCurrentIndex(idx);

        if (m_policyStatusLbl)
            m_policyStatusLbl->setText(
                QString(tr("%1 modelli")).arg(m_policyModelCombo->count()));
    });
    connect(m_ai, &AiClient::error, holder,
            [this, holder](const QString&) {
        holder->deleteLater();
        if (!m_policyModelCombo) return;
        m_policyModelCombo->clear();
        m_policyModelCombo->addItem("\xe2\x9d\x8c  Backend non raggiungibile", "");
        if (m_policyStatusLbl)
            m_policyStatusLbl->setText(tr("\xe2\x9d\x8c  Ollama non raggiungibile"));
    });
    m_ai->fetchModels();
}

/* ──────────────────────────────────────────────────────────────────────
   onPolicyGenerateClicked — genera la policy via AI
   ────────────────────────────────────────────────────────────────────── */
void ProgrammazionePage::onPolicyGenerateClicked()
{
    if (!m_policyDesc || !m_policyOutput || !m_policyTypeCombo) return;

    const QString desc = m_policyDesc->toPlainText().trimmed();
    if (desc.isEmpty()) {
        if (m_policyStatusLbl)
            m_policyStatusLbl->setText(
                tr("\xe2\x9a\xa0  Descrivi la policy prima di generare."));
        return;
    }

    const int idx = m_policyTypeCombo->currentIndex();
    if (idx < 0 || idx >= kN) return;

    /* System prompt specifico per il tipo selezionato */
    const QString sys = QString::fromLatin1(kTypes[idx].systemPrompt);
    const QString tipo = QString::fromUtf8(kTypes[idx].label);

    /* Override modello se selezionato un modello diverso da quello globale */
    const QString selectedModel =
        m_policyModelCombo ? m_policyModelCombo->currentData(Qt::UserRole).toString()
                           : QString();
    m_policyPrevModel = m_ai->model();
    if (!selectedModel.isEmpty() && selectedModel != m_policyPrevModel)
        m_ai->setModel(selectedModel);
    else
        m_policyPrevModel.clear();  /* nessun restore necessario */

    if (m_policyStatusLbl)
        m_policyStatusLbl->setText(
            tr("\xf0\x9f\xa4\x96  Generazione %1 in corso...").arg(tipo));
    m_policyOutput->clear();

    disconnect(m_policyAiTokConn);
    disconnect(m_policyAiFinConn);
    disconnect(m_policyAiErrConn);

    m_policyAiTokConn = connect(m_ai, &AiClient::token,
                                this, &ProgrammazionePage::onPolicyAiToken);
    m_policyAiFinConn = connect(m_ai, &AiClient::finished,
                                this, &ProgrammazionePage::onPolicyAiFinished);
    m_policyAiErrConn = connect(m_ai, &AiClient::error,
                                this, &ProgrammazionePage::onPolicyAiError);

    m_ai->chat(sys, desc);
    LogBus::post(QString("\xf0\x9f\x9b\xa1 Policy — generazione %1 avviata (modello: %2)")
                 .arg(tipo, m_ai->model()));
}

void ProgrammazionePage::onPolicyAiToken(const QString& t)
{
    if (!m_policyOutput) return;
    QTextCursor c = m_policyOutput->textCursor();
    c.movePosition(QTextCursor::End);
    c.insertText(t);
    m_policyOutput->setTextCursor(c);
}

void ProgrammazionePage::onPolicyAiFinished(const QString& /*full*/)
{
    disconnect(m_policyAiTokConn);
    disconnect(m_policyAiFinConn);
    disconnect(m_policyAiErrConn);

    if (!m_policyPrevModel.isEmpty()) {
        m_ai->setModel(m_policyPrevModel);
        m_policyPrevModel.clear();
    }

    if (m_policyStatusLbl)
        m_policyStatusLbl->setText(tr("\xe2\x9c\x85  Policy generata"));
    LogBus::post("\xe2\x9c\x85 Policy — generazione completata");
}

void ProgrammazionePage::onPolicyAiError(const QString& msg)
{
    disconnect(m_policyAiTokConn);
    disconnect(m_policyAiFinConn);
    disconnect(m_policyAiErrConn);

    if (!m_policyPrevModel.isEmpty()) {
        m_ai->setModel(m_policyPrevModel);
        m_policyPrevModel.clear();
    }

    if (m_policyStatusLbl)
        m_policyStatusLbl->setText(tr("\xe2\x9d\x8c  Errore AI"));
    if (m_policyOutput)
        m_policyOutput->append("\n# ERRORE: " + msg);
    LogBus::post("\xe2\x9d\x8c Policy — errore AI: " + msg);
}
