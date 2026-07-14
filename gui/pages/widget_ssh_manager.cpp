#include "widget_ssh_manager.h"
#include "../dpi_utils.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QGroupBox>
#include <QSplitter>
#include <QTableWidget>
#include <QHeaderView>
#include <QTextEdit>
#include <QLineEdit>
#include <QSpinBox>
#include <QComboBox>
#include <QPushButton>
#include <QLabel>
#include <QProgressBar>
#include <QCheckBox>
#include <QNetworkInterface>
#include <QHostAddress>
#include <QRegularExpression>
#include <QApplication>
#include <QClipboard>
#include <QFileDialog>
#include <QStandardPaths>
#include <QTimer>

static bool hasExe(const QString& name)
{
    return !QStandardPaths::findExecutable(name).isEmpty();
}

/* ════════════════════════════════════════════════════════════════════════ */

SshManagerWidget::SshManagerWidget(QWidget* parent)
    : QWidget(parent)
{
    auto* mainLay = new QVBoxLayout(this);
    mainLay->setContentsMargins(8, 8, 8, 8);
    mainLay->setSpacing(0);

    auto* splitter = new QSplitter(Qt::Vertical, this);
    mainLay->addWidget(splitter);

    /* ── SEZIONE DISCOVERY ──────────────────────────────────────────────── */
    auto* discBox = new QGroupBox(tr("Scansione rete locale"), splitter);
    auto* discLay = new QVBoxLayout(discBox);
    discLay->setSpacing(6);

    auto* scanRow = new QHBoxLayout;
    m_arpBtn  = new QPushButton(tr("\xf0\x9f\x93\xa1  Tabella ARP"),  discBox);  /* 📡 */
    m_nmapBtn = new QPushButton(tr("\xf0\x9f\x94\x8d  Scan completo"), discBox); /* 🔍 */
    m_stopBtn = new QPushButton(tr("\xe2\x9c\x96  Stop"),              discBox); /* ✖  */
    m_arpBtn->setToolTip(tr(
        "Legge istantaneamente la tabella ARP del kernel.\n"
        "Mostra solo host con cui hai gia comunicato di recente."));
    m_nmapBtn->setToolTip(tr(
        "Ping sweep della subnet (usa nmap se disponibile,\n"
        "altrimenti fping). Scopre tutti gli host attivi."));
    m_stopBtn->setEnabled(false);

    m_scanLbl = new QLabel(tr("Premi 'Tabella ARP' o 'Scan completo'"), discBox);
    m_scanLbl->setStyleSheet("color: #9e9e9e; font-style: italic;");

    scanRow->addWidget(m_arpBtn);
    scanRow->addWidget(m_nmapBtn);
    scanRow->addWidget(m_stopBtn);
    scanRow->addStretch();
    scanRow->addWidget(m_scanLbl);
    discLay->addLayout(scanRow);

    m_scanBar = new QProgressBar(discBox);
    m_scanBar->setRange(0, 0);
    m_scanBar->setFixedHeight(dpiScale(4));
    m_scanBar->setVisible(false);
    m_scanBar->setTextVisible(false);
    discLay->addWidget(m_scanBar);

    m_hostTable = new QTableWidget(0, 4, discBox);
    m_hostTable->setHorizontalHeaderLabels({tr("IP"), tr("Hostname"), tr("MAC"), tr("Interfaccia")});
    m_hostTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    m_hostTable->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
    m_hostTable->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    m_hostTable->horizontalHeader()->setSectionResizeMode(3, QHeaderView::ResizeToContents);
    m_hostTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_hostTable->setSelectionMode(QAbstractItemView::SingleSelection);
    m_hostTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_hostTable->setAlternatingRowColors(true);
    m_hostTable->verticalHeader()->setVisible(false);
    m_hostTable->setToolTip(tr("Doppio clic per usare l'host selezionato"));
    discLay->addWidget(m_hostTable);

    auto* useRow = new QHBoxLayout;
    auto* useBtn = new QPushButton(tr("\xe2\xac\x86  Usa selezionato"), discBox);
    useBtn->setObjectName("actionBtn");
    useRow->addWidget(useBtn);
    useRow->addStretch();
    auto* hintLbl = new QLabel(
        "<small><i>Seleziona una riga e premi 'Usa selezionato' "
        "oppure fai doppio clic</i></small>", discBox);
    hintLbl->setTextFormat(Qt::RichText);
    useRow->addWidget(hintLbl);
    discLay->addLayout(useRow);

    /* ── SEZIONE CONNESSIONE ─────────────────────────────────────────────── */
    auto* connBox = new QGroupBox(tr("Connessione SSH"), splitter);
    auto* connLay = new QVBoxLayout(connBox);
    connLay->setSpacing(6);

    auto* form = new QFormLayout;
    form->setLabelAlignment(Qt::AlignRight);
    form->setSpacing(5);

    m_hostEdit = new QLineEdit(connBox);
    m_hostEdit->setPlaceholderText(tr("es. 192.168.1.100"));
    form->addRow(tr("Host / IP:"), m_hostEdit);

    m_portSpin = new QSpinBox(connBox);
    m_portSpin->setRange(1, 65535);
    m_portSpin->setValue(22);
    form->addRow(tr("Porta:"), m_portSpin);

    m_userEdit = new QLineEdit(connBox);
    m_userEdit->setPlaceholderText(tr("nome utente"));
    {
        const QString u = qEnvironmentVariable("USER");
        if (!u.isEmpty()) m_userEdit->setText(u);
    }
    form->addRow(tr("Utente:"), m_userEdit);

    auto* keyW   = new QWidget(connBox);
    auto* keyRow = new QHBoxLayout(keyW);
    keyRow->setContentsMargins(0, 0, 0, 0);
    keyRow->setSpacing(4);
    m_keyEdit = new QLineEdit(keyW);
    m_keyEdit->setPlaceholderText(tr("~/.ssh/id_rsa  (vuoto = password interattiva)"));
    auto* keyBrowse = new QPushButton("\xe2\x80\xa6", keyW);
    keyBrowse->setFixedWidth(dpiScale(32));
    keyBrowse->setToolTip(tr("Sfoglia chiave privata SSH"));
    keyRow->addWidget(m_keyEdit);
    keyRow->addWidget(keyBrowse);
    form->addRow(tr("Chiave SSH:"), keyW);

    m_modeCombo = new QComboBox(connBox);
    m_modeCombo->addItem("\xf0\x9f\x96\xa5  Shell semplice",               "shell"); /* 🖥  */
    m_modeCombo->addItem("\xf0\x9f\xaa\x9f  Sessione X11  (ssh -X)",       "x11");   /* 🪟  */
    m_modeCombo->addItem("\xf0\x9f\xaa\x9f  Sessione X11 trusted (ssh -Y)","x11y");  /* 🪟  */
    m_modeCombo->addItem("\xf0\x9f\x8c\x8a  Wayland (waypipe)",            "wayl");  /* 🌊  */
    if (!hasExe("waypipe"))
        m_modeCombo->setItemText(3,
            "\xf0\x9f\x8c\x8a  Wayland (waypipe \xe2\x80\x94 non trovato)"); /* — */
    form->addRow(tr("Modalita:"), m_modeCombo);

    auto* optsW   = new QWidget(connBox);
    auto* optsRow = new QHBoxLayout(optsW);
    optsRow->setContentsMargins(0, 0, 0, 0);
    m_compCheck  = new QCheckBox(tr("Comprimi  (-C)"),  optsW);
    m_agentCheck = new QCheckBox(tr("Agente SSH (-A)"), optsW);
    m_compCheck->setToolTip(tr("Abilita la compressione gzip del traffico SSH"));
    m_agentCheck->setToolTip(tr("Propaga l'agente SSH locale (port-forward chiavi)"));
    optsRow->addWidget(m_compCheck);
    optsRow->addWidget(m_agentCheck);
    optsRow->addStretch();
    form->addRow(tr("Opzioni:"), optsW);

    connLay->addLayout(form);

    m_noteLbl = new QLabel(connBox);
    m_noteLbl->setWordWrap(true);
    m_noteLbl->setTextFormat(Qt::RichText);
    m_noteLbl->setVisible(false);
    connLay->addWidget(m_noteLbl);

    m_preview = new QTextEdit(connBox);
    m_preview->setReadOnly(true);
    m_preview->setFixedHeight(dpiScale(48));
    m_preview->setPlaceholderText(tr("Anteprima comando SSH..."));
    m_preview->setObjectName("codePreview");
    connLay->addWidget(m_preview);

    auto* btnRow = new QHBoxLayout;
    m_connBtn = new QPushButton(tr("\xf0\x9f\x94\x97  Connetti"), connBox);  /* 🔗 */
    m_copyBtn = new QPushButton(tr("\xf0\x9f\x93\x8b  Copia comando"), connBox); /* 📋 */
    m_connBtn->setObjectName("actionBtn");
    btnRow->addWidget(m_connBtn);
    btnRow->addWidget(m_copyBtn);
    btnRow->addStretch();
    connLay->addLayout(btnRow);
    connLay->addStretch();

    splitter->setStretchFactor(0, 3);
    splitter->setStretchFactor(1, 2);

    /* ── Connessioni ────────────────────────────────────────────────────── */
    connect(m_arpBtn,  &QPushButton::clicked, this, &SshManagerWidget::onArpClicked);
    connect(m_nmapBtn, &QPushButton::clicked, this, &SshManagerWidget::onNmapClicked);
    connect(m_stopBtn, &QPushButton::clicked, this, &SshManagerWidget::onStopClicked);
    connect(useBtn,    &QPushButton::clicked, this, &SshManagerWidget::onUseSelected);
    connect(m_hostTable, &QTableWidget::itemSelectionChanged,
            this, &SshManagerWidget::onUseSelected);
    connect(m_hostTable, &QTableWidget::cellDoubleClicked,
            this, [this](int, int){ onUseSelected(); });
    connect(m_modeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &SshManagerWidget::onModeChanged);
    connect(m_connBtn,  &QPushButton::clicked, this, &SshManagerWidget::onConnectClicked);
    connect(m_copyBtn,  &QPushButton::clicked, this, &SshManagerWidget::onCopyClicked);
    connect(keyBrowse,  &QPushButton::clicked, this, &SshManagerWidget::onKeyBrowseClicked);

    connect(m_hostEdit,  &QLineEdit::textChanged,
            this, &SshManagerWidget::updatePreview);
    connect(m_portSpin,  QOverload<int>::of(&QSpinBox::valueChanged),
            this, &SshManagerWidget::updatePreview);
    connect(m_userEdit,  &QLineEdit::textChanged,
            this, &SshManagerWidget::updatePreview);
    connect(m_keyEdit,   &QLineEdit::textChanged,
            this, &SshManagerWidget::updatePreview);
    connect(m_compCheck,  &QCheckBox::toggled, this, &SshManagerWidget::updatePreview);
    connect(m_agentCheck, &QCheckBox::toggled, this, &SshManagerWidget::updatePreview);

    updatePreview();
}

/* Stesso pattern già documentato in gui/CLAUDE.md dopo un coredump reale
   (~VoiceClonerWidget, fix D-20): m_scanProc è un QProcess figlio con
   segnali collegati a lambda che toccano m_hostTable/m_scanLbl — se ancora
   attivo alla chiusura, il suo ~QProcess emette finished() sincrono durante
   deleteChildren() su widget già semi-distrutti. findChildren() invece di
   solo m_scanProc copre anche eventuali QProcess anonimi. */
SshManagerWidget::~SshManagerWidget()
{
    const auto procs = findChildren<QProcess*>();
    for (QProcess* p : procs) {
        p->disconnect(this);
        p->blockSignals(true);
        if (p->state() != QProcess::NotRunning) {
            p->kill();
            p->waitForFinished(1000);
        }
    }
}

/* ── addOrUpdateHost ──────────────────────────────────────────────────── */
void SshManagerWidget::addOrUpdateHost(const QString& ip, const QString& host,
                                        const QString& mac, const QString& iface)
{
    for (int r = 0; r < m_hostTable->rowCount(); ++r) {
        auto* ipItem = m_hostTable->item(r, 0);
        if (!ipItem || ipItem->text() != ip) continue;
        if (!host.isEmpty() && host != "?" && host != ip)
            m_hostTable->item(r, 1)->setText(host);
        if (!mac.isEmpty() && mac != "<incomplete>")
            m_hostTable->item(r, 2)->setText(mac);
        if (!iface.isEmpty())
            m_hostTable->item(r, 3)->setText(iface);
        return;
    }
    const int r = m_hostTable->rowCount();
    m_hostTable->insertRow(r);
    m_hostTable->setItem(r, 0, new QTableWidgetItem(ip));
    const QString hn = (host.isEmpty() || host == "?") ? ip : host;
    m_hostTable->setItem(r, 1, new QTableWidgetItem(hn));
    m_hostTable->setItem(r, 2, new QTableWidgetItem(
        (mac.isEmpty() || mac == "<incomplete>") ? QString() : mac));
    m_hostTable->setItem(r, 3, new QTableWidgetItem(iface));
}

/* ── parseScanLines — processa righe complete dal buffer ───────────────── */
void SshManagerWidget::parseScanLines(const QString& text)
{
    /* arp -a:  hostname (IP) at MAC [ether] on iface
                ? (IP) at MAC [ether] on iface        */
    static const QRegularExpression reArp(
        R"(^(\S+)\s+\((\d+\.\d+\.\d+\.\d+)\)\s+at\s+(\S+).*\bon\s+(\S+))",
        QRegularExpression::MultilineOption);

    /* nmap: "Nmap scan report for HOST (IP)"  o  "Nmap scan report for IP" */
    static const QRegularExpression reNmapFull(
        R"(Nmap scan report for (\S+)\s+\((\d+\.\d+\.\d+\.\d+)\))");
    static const QRegularExpression reNmapIpOnly(
        R"(Nmap scan report for (\d+\.\d+\.\d+\.\d+))");
    /* nmap MAC line */
    static const QRegularExpression reNmapMac(
        R"(MAC Address:\s+([0-9A-Fa-f:]{17}))");
    /* fping: "IP is alive" */
    static const QRegularExpression reFping(
        R"(^(\d+\.\d+\.\d+\.\d+)\s+is alive)",
        QRegularExpression::MultilineOption);

    /* ── ARP (cerca in tutto il testo) ── */
    auto it = reArp.globalMatch(text);
    while (it.hasNext()) {
        auto m = it.next();
        addOrUpdateHost(m.captured(2), m.captured(1), m.captured(3), m.captured(4));
    }

    /* ── nmap (processa linea per linea per associare MAC all'IP) ── */
    QString lastIp;
    for (const QString& line : text.split('\n')) {
        auto m = reNmapFull.match(line);
        if (m.hasMatch()) {
            lastIp = m.captured(2);
            addOrUpdateHost(lastIp, m.captured(1), {}, {});
            continue;
        }
        m = reNmapIpOnly.match(line);
        if (m.hasMatch()) {
            lastIp = m.captured(1);
            addOrUpdateHost(lastIp, {}, {}, {});
            continue;
        }
        m = reNmapMac.match(line);
        if (m.hasMatch() && !lastIp.isEmpty()) {
            addOrUpdateHost(lastIp, {}, m.captured(1), {});
            continue;
        }
    }

    /* ── fping ── */
    auto fi = reFping.globalMatch(text);
    while (fi.hasNext()) {
        auto m = fi.next();
        addOrUpdateHost(m.captured(1), {}, {}, {});
    }
}

/* ── ARP scan ─────────────────────────────────────────────────────────── */
void SshManagerWidget::onArpClicked()
{
    if (m_scanProc && m_scanProc->state() != QProcess::NotRunning) return;
    m_hostTable->setRowCount(0);
    m_scanBuf.clear();
    m_scanLbl->setText(tr("Lettura tabella ARP..."));
    m_scanBar->setVisible(true);
    m_arpBtn->setEnabled(false);
    m_nmapBtn->setEnabled(false);
    m_stopBtn->setEnabled(true);

    m_scanProc = new QProcess(this);
    connect(m_scanProc, &QProcess::readyReadStandardOutput,
            this, &SshManagerWidget::onScanReadyRead);
    connect(m_scanProc, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, &SshManagerWidget::onScanFinished);
    connect(m_scanProc, &QProcess::errorOccurred,
            this, [this](QProcess::ProcessError err) {
        if (err == QProcess::FailedToStart)
            qWarning() << "[ssh_manager_arp] processo non avviato:" << m_scanProc->program();
    });
    m_scanProc->start("arp", {"-a"});
}

/* ── Scan completo (nmap / fping) ─────────────────────────────────────── */
void SshManagerWidget::onNmapClicked()
{
    if (m_scanProc && m_scanProc->state() != QProcess::NotRunning) return;

    /* ricava la prima subnet LAN disponibile */
    QString subnet;
    for (const QNetworkInterface& iface : QNetworkInterface::allInterfaces()) {
        if (iface.flags().testFlag(QNetworkInterface::IsLoopBack)) continue;
        if (!iface.flags().testFlag(QNetworkInterface::IsUp))      continue;
        for (const QNetworkAddressEntry& e : iface.addressEntries()) {
            if (e.ip().protocol() != QAbstractSocket::IPv4Protocol) continue;
            const QString s = e.ip().toString();
            if (!s.startsWith("192.168.") && !s.startsWith("10.")
                    && !s.startsWith("172.")) continue;
            const int pfx = (e.prefixLength() > 0) ? e.prefixLength() : 24;
            subnet = s.section('.', 0, 2) + ".0/" + QString::number(pfx);
            break;
        }
        if (!subnet.isEmpty()) break;
    }

    if (subnet.isEmpty()) {
        m_scanLbl->setText(tr("\xe2\x9d\x8c  Nessuna interfaccia LAN IPv4 rilevata"));
        return;
    }

    m_hostTable->setRowCount(0);
    m_scanBuf.clear();
    m_scanBar->setVisible(true);
    m_arpBtn->setEnabled(false);
    m_nmapBtn->setEnabled(false);
    m_stopBtn->setEnabled(true);

    m_scanProc = new QProcess(this);
    connect(m_scanProc, &QProcess::readyReadStandardOutput,
            this, &SshManagerWidget::onScanReadyRead);
    connect(m_scanProc, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, &SshManagerWidget::onScanFinished);
    connect(m_scanProc, &QProcess::errorOccurred,
            this, [this](QProcess::ProcessError err) {
        if (err == QProcess::FailedToStart)
            qWarning() << "[ssh_manager_nmap] processo non avviato:" << m_scanProc->program();
    });

    if (hasExe("nmap")) {
        m_scanLbl->setText(tr("nmap -sn ") + subnet + "  ...");
        m_scanProc->start("nmap", {"-sn", subnet});
    } else if (hasExe("fping")) {
        m_scanLbl->setText(tr("fping -a -g ") + subnet + "  ...");
        m_scanProc->setProcessChannelMode(QProcess::MergedChannels);
        m_scanProc->start("fping", {"-a", "-g", subnet});
    } else {
        m_scanBar->setVisible(false);
        m_arpBtn->setEnabled(true);
        m_nmapBtn->setEnabled(true);
        m_stopBtn->setEnabled(false);
        m_scanProc->deleteLater();
        m_scanLbl->setText(tr(
            "\xe2\x9d\x8c  Installa nmap o fping:  "
            "sudo apt install nmap"));
    }
}

void SshManagerWidget::onStopClicked()
{
    if (m_scanProc) m_scanProc->kill();
}

/* ── onScanReadyRead — bufferizza e processa righe complete ────────────── */
void SshManagerWidget::onScanReadyRead()
{
    if (!m_scanProc) return;
    m_scanBuf += QString::fromLocal8Bit(m_scanProc->readAllStandardOutput());
    const int last = m_scanBuf.lastIndexOf('\n');
    if (last < 0) return;
    parseScanLines(m_scanBuf.left(last + 1));
    m_scanBuf = m_scanBuf.mid(last + 1);
}

void SshManagerWidget::onScanFinished(int, QProcess::ExitStatus)
{
    /* flush buffer residuo */
    if (!m_scanBuf.isEmpty()) {
        parseScanLines(m_scanBuf);
        m_scanBuf.clear();
    }
    m_scanBar->setVisible(false);
    m_arpBtn->setEnabled(true);
    m_nmapBtn->setEnabled(true);
    m_stopBtn->setEnabled(false);
    m_scanLbl->setText(
        QString(tr("\xe2\x9c\x85  %1 host trovati")).arg(m_hostTable->rowCount()));
    if (m_scanProc) { m_scanProc->deleteLater(); m_scanProc = nullptr; }
}

/* ── Selezione riga → popola form ─────────────────────────────────────── */
void SshManagerWidget::onUseSelected()
{
    const auto sel = m_hostTable->selectedItems();
    if (sel.isEmpty()) return;
    const int row = sel.first()->row();
    if (auto* it = m_hostTable->item(row, 0))
        m_hostEdit->setText(it->text());
}

/* ── Whitelist campi SSH ──────────────────────────────────────────────────
   buildCmd() produce una riga che finisce dentro `bash -c '...'` in un
   terminale: host/utente/chiave devono restare DATI, mai sintassi shell.
   Strategia a rifiuto esplicito (non sanificazione per rimozione, che
   lascia varchi): campo fuori formato → connessione bloccata con messaggio.
   Il '-' iniziale è respinto a parte: ssh lo leggerebbe come opzione
   (argv injection, es. host "-oProxyCommand=..."), senza bisogno di
   alcun metacarattere shell. */
static bool sshTokenOk(const QString& s, const QRegularExpression& re)
{
    return !s.startsWith(QLatin1Char('-')) && re.match(s).hasMatch();
}

/* ── Blacklist metacaratteri shell (seconda rete) ─────────────────────────
   La whitelist resta la difesa primaria: da sola basta. Questa blacklist
   è un invariante indipendente — se un domani la whitelist venisse
   allargata per un caso legittimo, i metacaratteri shell resterebbero
   comunque respinti qui. Non può MAI sostituire la whitelist (una
   blacklist da sola vieta solo ciò a cui si è pensato). Ritorna il primo
   carattere vietato trovato (QChar nullo = campo pulito), così il
   messaggio d'errore può nominarlo. */
static QChar sshFirstForbiddenChar(const QString& s)
{
    static const QString kForbidden =
        QStringLiteral("'\"`$;|&<>(){}[]*?!#\\ \t");
    for (const QChar c : s)
        if (c.unicode() < 0x20 || kForbidden.contains(c))
            return c;
    return {};
}

QString SshManagerWidget::invalidFieldError() const
{
    /* IP, IPv6 (con ':'), hostname — niente spazi, quote o metacaratteri */
    static const QRegularExpression reHost(QStringLiteral("^[A-Za-z0-9._:\\-]+$"));
    static const QRegularExpression reUser(QStringLiteral("^[A-Za-z0-9._\\-]+$"));
    /* Path chiave: assoluto o ~/ — gli spazi romperebbero comunque la riga */
    static const QRegularExpression reKey(QStringLiteral("^[A-Za-z0-9._/~\\-]+$"));

    const QString host = m_hostEdit->text().trimmed();
    const QString user = m_userEdit->text().trimmed();
    const QString key  = m_keyEdit->text().trimmed();

    /* Prima la blacklist: se scatta, il messaggio nomina il carattere
       incriminato — più utile del generico "campo non valido". */
    const struct { QString label; const QString& value; } fields[] = {
        { tr("Host"),            host },
        { tr("Utente"),          user },
        { tr("Percorso chiave"), key  },
    };
    for (const auto& f : fields) {
        const QChar bad = sshFirstForbiddenChar(f.value);
        if (bad.isNull()) continue;
        if (bad.unicode() < 0x20 || bad == QLatin1Char(' ') || bad == QLatin1Char('\t'))
            return tr("%1: contiene spazi o caratteri di controllo").arg(f.label);
        return tr("%1: carattere vietato \xc2\xab%2\xc2\xbb "
                  "(metacarattere shell)").arg(f.label).arg(bad);
    }

    if (!host.isEmpty() && !sshTokenOk(host, reHost))
        return tr("Host non valido: ammessi solo lettere, numeri e . : - _ "
                  "(niente '-' iniziale)");
    if (!user.isEmpty() && !sshTokenOk(user, reUser))
        return tr("Utente non valido: ammessi solo lettere, numeri e . - _ "
                  "(niente '-' iniziale)");
    if (!key.isEmpty() && !sshTokenOk(key, reKey))
        return tr("Percorso chiave non valido: niente spazi, apici o "
                  "caratteri speciali \xe2\x80\x94 sposta la chiave in un "
                  "percorso semplice (es. ~/.ssh/)");
    return {};
}

/* ── buildCmd ─────────────────────────────────────────────────────────── */
QString SshManagerWidget::buildCmd() const
{
    const QString host = m_hostEdit->text().trimmed();
    const QString user = m_userEdit->text().trimmed();
    const int     port = m_portSpin->value();
    const QString key  = m_keyEdit->text().trimmed();
    const QString mode = m_modeCombo->currentData().toString();

    QStringList args;
    if (mode == "wayl") args << "waypipe";
    args << "ssh";
    if (mode == "x11")  args << "-X";
    if (mode == "x11y") args << "-Y";
    if (m_compCheck->isChecked())  args << "-C";
    if (m_agentCheck->isChecked()) args << "-A";
    if (port != 22) args << "-p" << QString::number(port);
    if (!key.isEmpty()) args << "-i" << key;
    const QString dest = user.isEmpty() ? host : user + "@" + host;
    args << dest;
    return args.join(' ');
}

/* ── updatePreview ────────────────────────────────────────────────────── */
void SshManagerWidget::updatePreview()
{
    m_preview->setPlainText(buildCmd());

    const QString mode = m_modeCombo->currentData().toString();
    if (mode == "wayl") {
        if (!hasExe("waypipe")) {
            m_noteLbl->setText(
                "<small><i>\xe2\x9a\xa0  waypipe non trovato. "
                "Installa con: <b>sudo apt install waypipe</b><br>"
                "Permette il forwarding di app Wayland native via SSH.</i></small>");
            m_noteLbl->setVisible(true);
        } else {
            m_noteLbl->setVisible(false);
        }
    } else if (mode == "x11" || mode == "x11y") {
        m_noteLbl->setText(
            "<small><i>\xf0\x9f\x93\x8c  Richiede un server X11 attivo sul client "
            "(Xorg o XWayland). Su Wayland puro potrebbe richiedere Xwayland attivo."
            "</i></small>");
        m_noteLbl->setVisible(true);
    } else {
        m_noteLbl->setVisible(false);
    }
}

void SshManagerWidget::onModeChanged(int)
{
    updatePreview();
}

/* ── findTerminal ─────────────────────────────────────────────────────── */
QString SshManagerWidget::findTerminal()
{
    for (const QString& t : {"gnome-terminal", "konsole", "xfce4-terminal",
                              "mate-terminal",  "tilix",   "alacritty",
                              "kitty",          "lxterminal", "xterm"}) {
        if (hasExe(t)) return t;
    }
    return {};
}

/* ── onConnectClicked ─────────────────────────────────────────────────── */
void SshManagerWidget::onConnectClicked()
{
    const QString host = m_hostEdit->text().trimmed();
    if (host.isEmpty()) {
        m_scanLbl->setText(tr("\xe2\x9d\x8c  Inserisci un host o selezionalo dalla tabella"));
        return;
    }

    const QString fieldErr = invalidFieldError();
    if (!fieldErr.isEmpty()) {
        m_scanLbl->setText(tr("\xe2\x9d\x8c  ") + fieldErr);
        return;
    }

    const QString term = findTerminal();
    if (term.isEmpty()) {
        m_scanLbl->setText(tr(
            "\xe2\x9d\x8c  Nessun terminale trovato. "
            "Installa xterm:  sudo apt install xterm"));
        return;
    }

    const QString cmd = buildCmd();
    /* bash -c "CMD; exec bash"  mantiene il terminale aperto dopo la disconnessione.
       La concatenazione con apici è sicura SOLO perché invalidFieldError()
       (chiamata sopra) garantisce che host/utente/chiave non contengano
       apici né altri metacaratteri shell. */
    const QString shell = "bash -c '" + cmd + "; exec bash'";

    QStringList termArgs;
    if (term == "gnome-terminal") {
        termArgs << "--" << "bash" << "-c" << cmd + "; exec bash";
    } else if (term == "konsole") {
        termArgs << "--hold" << "-e" << cmd;
    } else if (term == "alacritty" || term == "kitty") {
        termArgs << "-e" << "bash" << "-c" << cmd + "; exec bash";
    } else {
        /* xterm, xfce4-terminal, mate-terminal, tilix, lxterminal */
        termArgs << "-e" << shell;
    }

    QProcess::startDetached(term, termArgs);
    m_scanLbl->setText(
        "\xf0\x9f\x94\x97  " + term + "  \xe2\x86\x92  " + cmd);  /* 🔗 → */
}

/* ── onCopyClicked ────────────────────────────────────────────────────── */
void SshManagerWidget::onCopyClicked()
{
    /* Stessa whitelist di onConnectClicked: il comando copiato finisce
       incollato in un terminale, la superficie è identica. */
    const QString fieldErr = invalidFieldError();
    if (!fieldErr.isEmpty()) {
        m_scanLbl->setText(tr("\xe2\x9d\x8c  ") + fieldErr);
        return;
    }
    QApplication::clipboard()->setText(buildCmd());
    m_copyBtn->setText(tr("\xe2\x9c\x85  Copiato!"));
    QTimer::singleShot(2000, m_copyBtn,
        [this]{ if (m_copyBtn) m_copyBtn->setText(tr("\xf0\x9f\x93\x8b  Copia comando")); });
}

/* ── onKeyBrowseClicked ───────────────────────────────────────────────── */
void SshManagerWidget::onKeyBrowseClicked()
{
    const QString home = QStandardPaths::writableLocation(QStandardPaths::HomeLocation);
    const QString f = QFileDialog::getOpenFileName(this,
        tr("Seleziona chiave privata SSH"),
        home + "/.ssh",
        tr("Tutti i file (*)"));
    if (!f.isEmpty()) m_keyEdit->setText(f);
}
