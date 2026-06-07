/* ======================================================================
   main_net_policy.cpp — Generatore di Policy di rete/sicurezza con AI

   Sub-tab "🛡️ Policy" di Rete & Network in ProgrammazionePage.
   Supporta: IPsec (strongSwan), iptables, nftables, UFW, WireGuard ACL,
             n2n community policy.
   ====================================================================== */
#include "main_programming.h"
#include "../log_bus.h"
#include "../dpi_utils.h"

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

/* ──────────────────────────────────────────────────────────────────────
   buildPolicyTab — costruisce il sub-tab Policy Generator
   ────────────────────────────────────────────────────────────────────── */
QWidget* ProgrammazionePage::buildPolicyTab(QWidget* parent)
{
    static const struct { const char* label; const char* hint; const char* example; } kTypes[] = {
        {
            "IPsec / strongSwan",
            "\xf0\x9f\x94\x90 Genera /etc/ipsec.conf + /etc/ipsec.secrets per IKEv2 site-to-site o road-warrior.",
            "Connessione IKEv2 site-to-site tra 10.0.1.0/24 e 10.0.2.0/24. "
            "Server A: 1.2.3.4 — Server B: 5.6.7.8. Autenticazione con PSK."
        },
        {
            "iptables",
            "\xf0\x9f\x9b\xa1  Regole iptables per Linux. Genera catene INPUT/OUTPUT/FORWARD con politica DROP di default.",
            "Permetti SSH (22), HTTP (80), HTTPS (443) in ingresso. "
            "Blocca tutto il resto. Consenti traffico WireGuard UDP 51820. Log dei pacchetti bloccati."
        },
        {
            "nftables",
            "\xf0\x9f\x94\xa5 nftables — sostituto moderno di iptables. Sintassi compatta e performante.",
            "Firewall per server VPN: accetta traffico n2n UDP 7654, "
            "WireGuard UDP 51820, SSH 22. Blocca il resto in input."
        },
        {
            "UFW (Ubuntu)",
            "\xf0\x9f\x94\xb5 Uncomplicated Firewall — comandi ufw pronti all'uso per Ubuntu/Debian.",
            "Abilita UFW. Permetti SSH, HTTP, HTTPS. "
            "Abilita porta 11600/tcp per WAN Prismalux. Blocca ping ICMP."
        },
        {
            "WireGuard ACL",
            "\xf0\x9f\x94\x97 AllowedIPs per-peer in WireGuard — controlla quale traffico passa tra i peer.",
            "Peer A (10.0.0.2): accede solo a 10.0.0.1 e 192.168.1.0/24. "
            "Peer B (10.0.0.3): accesso completo 0.0.0.0/0. "
            "Peer C (10.0.0.4): solo porta 11600 verso 10.0.0.1."
        },
        {
            "n2n Community Policy",
            "\xf0\x9f\x8c\x90 Script bash per gestire community, chiavi e firewall n2n sul supernode.",
            "Crea due community separate: 'prismalux_prod' e 'prismalux_dev'. "
            "Limita 'prod' a max 10 edge. Aggiungi regola iptables per isolare le due community."
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

    auto* typeLbl = new QLabel("\xf0\x9f\x9b\xa1  Tipo:", typeRow);
    m_policyTypeCombo = new QComboBox(typeRow);
    m_policyTypeCombo->setFixedWidth(dpiScale(200));
    for (int i = 0; i < kN; ++i)
        m_policyTypeCombo->addItem(QString::fromUtf8(kTypes[i].label));

    m_policyStatusLbl = new QLabel(typeRow);
    m_policyStatusLbl->setObjectName("hintLabel");
    m_policyStatusLbl->setWordWrap(false);

    typeHL->addWidget(typeLbl);
    typeHL->addWidget(m_policyTypeCombo);
    typeHL->addStretch();
    typeHL->addWidget(m_policyStatusLbl);
    lay->addWidget(typeRow);

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
    m_policyDesc->setPlaceholderText(tr("Es: Permetti SSH e HTTPS in entrata, blocca tutto il resto..."));
    m_policyDesc->setPlainText(QString::fromUtf8(kTypes[0].example));
    m_policyDesc->setFixedHeight(dpiScale(90));
    m_policyDesc->setFont(QFont("JetBrains Mono,Fira Code,Consolas,Monospace", 9));
    descLay->addWidget(m_policyDesc);
    lay->addWidget(descGroup);

    /* ── Output policy ── */
    auto* outGroup = new QGroupBox(
        "\xf0\x9f\x93\x84  Policy generata (modificabile)", w);
    auto* outLay = new QVBoxLayout(outGroup);
    m_policyOutput = new QTextEdit(outGroup);
    m_policyOutput->setFont(QFont("JetBrains Mono,Fira Code,Consolas,Monospace", 9));
    m_policyOutput->setMinimumHeight(dpiScale(200));
    m_policyOutput->setPlaceholderText(tr("La policy generata dall'AI apparirà qui..."));
    outLay->addWidget(m_policyOutput);
    lay->addWidget(outGroup, 1);

    /* ── Barra azioni ── */
    auto* actRow = new QWidget(w);
    auto* actHL  = new QHBoxLayout(actRow);
    actHL->setContentsMargins(0, 0, 0, 0);
    actHL->setSpacing(8);

    auto* btnGen  = new QPushButton("\xf0\x9f\xa4\x96  Genera policy", actRow);
    btnGen->setObjectName("actionBtn");
    auto* btnCopy = new QPushButton("\xf0\x9f\x93\x8b  Copia", actRow);
    auto* btnStop = new QPushButton("\xe2\x8f\xb9  Stop", actRow);
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
            hintLbl,
            [hintLbl](int idx) {
        static const char* hints[] = {
            "\xf0\x9f\x94\x90 Genera /etc/ipsec.conf + /etc/ipsec.secrets per IKEv2 site-to-site o road-warrior.",
            "\xf0\x9f\x9b\xa1  Regole iptables per Linux. Genera catene INPUT/OUTPUT/FORWARD con politica DROP di default.",
            "\xf0\x9f\x94\xa5 nftables — sostituto moderno di iptables. Sintassi compatta e performante.",
            "\xf0\x9f\x94\xb5 Uncomplicated Firewall — comandi ufw pronti all'uso per Ubuntu/Debian.",
            "\xf0\x9f\x94\x97 AllowedIPs per-peer in WireGuard — controlla quale traffico passa tra i peer.",
            "\xf0\x9f\x8c\x90 Script bash per gestire community, chiavi e firewall n2n sul supernode.",
        };
        static const char* examples[] = {
            "Connessione IKEv2 site-to-site tra 10.0.1.0/24 e 10.0.2.0/24. "
            "Server A: 1.2.3.4 — Server B: 5.6.7.8. Autenticazione con PSK.",
            "Permetti SSH (22), HTTP (80), HTTPS (443) in ingresso. "
            "Blocca tutto il resto. Consenti traffico WireGuard UDP 51820. Log dei pacchetti bloccati.",
            "Firewall per server VPN: accetta traffico n2n UDP 7654, "
            "WireGuard UDP 51820, SSH 22. Blocca il resto in input.",
            "Abilita UFW. Permetti SSH, HTTP, HTTPS. "
            "Abilita porta 11600/tcp per WAN Prismalux. Blocca ping ICMP.",
            "Peer A (10.0.0.2): accede solo a 10.0.0.1 e 192.168.1.0/24. "
            "Peer B (10.0.0.3): accesso completo 0.0.0.0/0.",
            "Crea due community separate: 'prismalux_prod' e 'prismalux_dev'. "
            "Limita 'prod' a max 10 edge.",
        };
        if (idx >= 0 && idx < 6) {
            hintLbl->setText(QString::fromUtf8(hints[idx]));
        }
        (void)examples;
    });

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
        disconnect(m_policyAiTokConn);
        disconnect(m_policyAiFinConn);
        disconnect(m_policyAiErrConn);
        btnGen->setEnabled(true);
        btnStop->setEnabled(false);
        if (m_policyStatusLbl) m_policyStatusLbl->setText(tr("\xe2\x8f\xb9  Fermato"));
    });

    return w;
}

/* ──────────────────────────────────────────────────────────────────────
   Slot: genera la policy via AI
   ────────────────────────────────────────────────────────────────────── */
void ProgrammazionePage::onPolicyGenerateClicked()
{
    if (!m_policyDesc || !m_policyOutput || !m_policyTypeCombo) return;

    const QString desc = m_policyDesc->toPlainText().trimmed();
    if (desc.isEmpty()) {
        if (m_policyStatusLbl)
            m_policyStatusLbl->setText(tr("\xe2\x9a\xa0  Descrivi la policy prima di generare."));
        return;
    }

    const QString tipo = m_policyTypeCombo->currentText();

    const QString sys =
        "Sei un esperto di sicurezza reti Linux. "
        "Genera una policy " + tipo + " completa e sicura basata sulla descrizione fornita. "
        "Regole generali:\n"
        "- Politica di default DROP/DENY su tutte le catene\n"
        "- Accetta solo il traffico esplicitamente richiesto\n"
        "- Aggiungi commenti inline che spiegano ogni regola\n"
        "- Il codice deve essere pronto per essere applicato su Linux senza modifiche\n"
        "Rispondi SOLO con il file di configurazione o lo script, senza spiegazioni aggiuntive.";

    if (m_policyStatusLbl)
        m_policyStatusLbl->setText(tr("\xf0\x9f\xa4\x96  Generazione in corso..."));
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
    LogBus::post(QString("\xf0\x9f\x9b\xa1 Policy — generazione %1 avviata").arg(tipo));
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
    if (m_policyStatusLbl)
        m_policyStatusLbl->setText(tr("\xe2\x9c\x85  Policy generata"));
    LogBus::post("\xe2\x9c\x85 Policy — generazione completata");
}

void ProgrammazionePage::onPolicyAiError(const QString& msg)
{
    disconnect(m_policyAiTokConn);
    disconnect(m_policyAiFinConn);
    disconnect(m_policyAiErrConn);
    if (m_policyStatusLbl)
        m_policyStatusLbl->setText(tr("\xe2\x9d\x8c  Errore AI"));
    if (m_policyOutput)
        m_policyOutput->append("\n# ERRORE: " + msg);
    LogBus::post("\xe2\x9d\x8c Policy — errore AI: " + msg);
}
