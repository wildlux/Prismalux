/* ══════════════════════════════════════════════════════════════
   main_programming_network.cpp — ProgrammazionePage: Network Analyzer + LAN Scanner
   ======================================================================================
   Sub-tab "🔬 Network Analyzer" e "🌐 Rete LAN" — builder UI + slot.
   lanRefreshInfo() riaccorpata qui insieme a buildReteLan() (nel sorgente
   originale erano fisicamente separate da 900+ righe per via del blocco
   Driver&Kernel in mezzo). Split da
   main_programming.cpp/main_programming_slots.cpp (TODO D-8).
   ══════════════════════════════════════════════════════════════ */
#include "main_programming.h"
#include "main_programming_p.h"
#include "../prismalux_paths.h"
#include "../ai_utils.h"
#include "../log_bus.h"
#include "../dpi_utils.h"
#include "../widgets/proc_helper.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QComboBox>
#include <QPlainTextEdit>
#include <QTextEdit>
#include <QProcess>
#include <QSpinBox>
#include <QSplitter>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QTabWidget>
#include <QAbstractItemView>
#include <QFont>
#include <QHeaderView>
#include <QNetworkInterface>
#include <QHostAddress>
#include <QAbstractSocket>
#include <QStandardPaths>
#include <QTimer>
#include <QTextCursor>
#include <QRegularExpression>

namespace P = PrismaluxPaths;

/* Rete & Network — widget autonomo cedibile a LanWanPage */
QWidget* ProgrammazionePage::buildReteNetworkWidget(QWidget* parent)
{
    auto* reteWrap = new QWidget(parent);
    auto* reteLay  = new QVBoxLayout(reteWrap);
    reteLay->setContentsMargins(0, 0, 0, 0);
    reteLay->setSpacing(0);
    auto* reteTabs = new QTabWidget(reteWrap);
    reteTabs->setObjectName("innerTabs");
    reteTabs->addTab(buildNetworkAnalyzer(reteTabs), "\xf0\x9f\x94\xa1  Cattura pacchetti");
    reteTabs->addTab(buildReteLan(reteTabs),         "\xf0\x9f\x8c\x90  Scan LAN");
    reteTabs->addTab(buildVpnTab(reteTabs),          "\xf0\x9f\x94\x92  VPN & Tunnel");
    reteTabs->addTab(buildPolicyTab(reteTabs),       "\xf0\x9f\x9b\xa1  Policy");
    reteTabs->addTab(buildSubnetTab(reteTabs),       "\xf0\x9f\x94\xa2  Sottoreti");
    reteLay->addWidget(reteTabs);
    return reteWrap;
}

/* ══════════════════════════════════════════════════════════════
   Network Analyzer — tshark/tcpdump wrapper + AI analysis
   ══════════════════════════════════════════════════════════════ */
QWidget* ProgrammazionePage::buildNetworkAnalyzer(QWidget* parent)
{
    /* Rileva tool prima di buildNetToolbar (serve per il tooltip Fix permessi) */
    m_netTool = QStandardPaths::findExecutable("tshark");
    if (m_netTool.isEmpty()) m_netTool = QStandardPaths::findExecutable("tcpdump");

    auto* w   = new QWidget(parent);
    auto* lay = new QVBoxLayout(w);
    lay->setContentsMargins(8, 8, 8, 8);
    lay->setSpacing(6);

    buildNetToolbar(lay, w);
    lay->addWidget(buildNetLogSplitter(w), 1);

    /* Stato tool */
    if (m_netTool.isEmpty()) {
        m_netStatus->setText(
            tr("\xe2\x9d\x8c  Nessun tool trovato. Installa tshark: sudo apt install tshark"));
        m_btnNetStart->setEnabled(false);
    } else {
        m_netStatus->setText(
            QString("\xe2\x9c\x85  Tool rilevato: %1").arg(m_netTool));
    }

    setupNetConnections();
    return w;
}

/* ── Toolbar: interfacce, protocollo, porta, max, pulsanti, status ── */
void ProgrammazionePage::buildNetToolbar(QVBoxLayout* lay, QWidget* w)
{
    auto* toolRow = new QHBoxLayout;
    toolRow->setSpacing(6);

    toolRow->addWidget(new QLabel(tr("\xf0\x9f\x93\xa1  Interfaccia:"), w));
    m_netIface = new QComboBox(w);
    m_netIface->setMinimumWidth(110);
    for (const QNetworkInterface& iface : QNetworkInterface::allInterfaces()) {
        if (iface.flags().testFlag(QNetworkInterface::IsLoopBack)) continue;
        const QString n = iface.name();
        m_netIface->addItem(AiUtils::ifaceEmoji(n) + n, n);
    }
    if (m_netIface->count() == 0) m_netIface->addItem("\xf0\x9f\x8c\x90  any", "any");
    toolRow->addWidget(m_netIface);

    toolRow->addWidget(new QLabel(tr("Protocollo:"), w));
    m_netProto = new QComboBox(w);
    m_netProto->addItem("Tutti",  QString(""));
    m_netProto->addItem("TCP",    QString("tcp"));
    m_netProto->addItem("UDP",    QString("udp"));
    m_netProto->addItem("ICMP",   QString("icmp"));
    m_netProto->addItem("ARP",    QString("arp"));
    m_netProto->addItem("DNS",    QString("udp port 53"));
    m_netProto->addItem("HTTP",   QString("tcp port 80"));
    m_netProto->addItem("HTTPS",  QString("tcp port 443"));
    m_netProto->setMinimumWidth(90);
    toolRow->addWidget(m_netProto);

    toolRow->addWidget(new QLabel(tr("Porta:"), w));
    m_netPort = new QSpinBox(w);
    m_netPort->setRange(0, 65535);
    m_netPort->setValue(0);
    m_netPort->setSpecialValueText("Tutte");
    m_netPort->setFixedWidth(dpiScale(80));
    m_netPort->setToolTip(tr("0 = tutte le porte"));
    toolRow->addWidget(m_netPort);

    toolRow->addWidget(new QLabel(tr("Max:"), w));
    m_netMaxPkts = new QSpinBox(w);
    m_netMaxPkts->setRange(10, 5000);
    m_netMaxPkts->setValue(200);
    m_netMaxPkts->setFixedWidth(dpiScale(70));
    toolRow->addWidget(m_netMaxPkts);

    m_btnNetStart    = new QPushButton(tr("\xe2\x96\xb6  Start"),          w);
    m_btnNetStop     = new QPushButton(tr("\xe2\x96\xa0  Stop"),           w);
    m_btnNetClear    = new QPushButton(tr("\xf0\x9f\x97\x91  Clear"),      w);
    m_btnNetAnalyze  = new QPushButton(tr("\xf0\x9f\xa4\x96  Analisi AI"), w);
    m_btnNetFixPerms = new QPushButton(tr("\xf0\x9f\x94\x91  Fix permessi"), w);
    m_btnNetStop->setEnabled(false);
    m_btnNetAnalyze->setEnabled(false);
    m_btnNetFixPerms->setToolTip(
        "Applica CAP_NET_RAW al tool di cattura (richiede password amministratore).\n"
        "In alternativa: sudo setcap cap_net_raw+eip " + m_netTool);
    m_btnNetFixPerms->setVisible(
        !QStandardPaths::findExecutable("pkexec").isEmpty());
    toolRow->addWidget(m_btnNetStart);
    toolRow->addWidget(m_btnNetStop);
    toolRow->addWidget(m_btnNetClear);
    toolRow->addWidget(m_btnNetAnalyze);
    toolRow->addWidget(m_btnNetFixPerms);
    lay->addLayout(toolRow);

    m_netStatus = new QLabel(w);
    m_netStatus->setObjectName("statusLabel");
    lay->addWidget(m_netStatus);
}

/* ── Splitter log pacchetti + output AI ── */
QWidget* ProgrammazionePage::buildNetLogSplitter(QWidget* parent)
{
    auto* splitter = new QSplitter(Qt::Vertical, parent);

    m_netLog = new QPlainTextEdit(splitter);
    m_netLog->setReadOnly(true);
    m_netLog->setMaximumBlockCount(5000);
    QFont mono("JetBrains Mono", 9);
    mono.setStyleHint(QFont::Monospace);
    m_netLog->setFont(mono);
    m_netLog->setPlaceholderText(tr("I pacchetti catturati appariranno qui..."));
    splitter->addWidget(m_netLog);

    m_netAiOutput = new QTextEdit(splitter);
    m_netAiOutput->setReadOnly(true);
    m_netAiOutput->setPlaceholderText(
        tr("\xf0\x9f\xa4\x96  L'analisi AI apparira' qui dopo aver cliccato 'Analisi AI'..."));
    splitter->addWidget(m_netAiOutput);
    splitter->setSizes({300, 150});
    return splitter;
}

/* ── Connessioni Network Analyzer ── */
void ProgrammazionePage::setupNetConnections()
{
    connect(m_btnNetStart,    &QPushButton::clicked, this, &ProgrammazionePage::netStart);
    connect(m_btnNetStop,     &QPushButton::clicked, this, &ProgrammazionePage::netStop);
    connect(m_btnNetClear,    &QPushButton::clicked, this, &ProgrammazionePage::onBtnNetClearClicked);
    connect(m_btnNetAnalyze,  &QPushButton::clicked, this, &ProgrammazionePage::netAiAnalyze);
    connect(m_btnNetFixPerms, &QPushButton::clicked, this, &ProgrammazionePage::netFixPermissions);
}

void ProgrammazionePage::netStart()
{
    if (m_netProc && m_netProc->state() != QProcess::NotRunning) return;

    const QString iface  = m_netIface->currentData().toString();
    const int     maxPkt = m_netMaxPkts->value();

    /* Costruisce il filtro BPF dai campi strutturati */
    QString filter = m_netProto->currentData().toString(); // es. "tcp", "udp port 53", ""
    const int port = m_netPort->value();
    if (port > 0) {
        /* Se il filtro contiene già "port" (DNS/HTTP/HTTPS), ignora il campo porta */
        if (!filter.contains("port")) {
            const QString proto = filter.isEmpty() ? "" : filter + " ";
            filter = proto + "port " + QString::number(port);
        }
    }

    m_netLog->clear();
    m_netAiOutput->clear();
    m_btnNetAnalyze->setEnabled(false);

    if (!m_netProc) m_netProc = new QProcess(this);

    QStringList args;
    m_netUseTshark = m_netTool.contains("tshark");

    if (m_netUseTshark) {
        args << "-i" << iface
             << "-c" << QString::number(maxPkt)
             << "-T" << "fields"
             << "-e" << "frame.number"
             << "-e" << "ip.src"
             << "-e" << "ip.dst"
             << "-e" << "_ws.col.Protocol"
             << "-e" << "frame.len"
             << "-e" << "_ws.col.Info"
             << "-E" << "separator=|"
             << "-l";
        if (!filter.isEmpty()) args << "-Y" << filter;
    } else {
        args << "-i" << iface
             << "-c" << QString::number(maxPkt)
             << "-l" << "-n" << "-q";
        if (!filter.isEmpty())
            args << filter.split(' ', Qt::SkipEmptyParts);
    }

    connect(m_netProc, &QProcess::readyReadStandardOutput,
            this, &ProgrammazionePage::onNetReadyRead);
    connect(m_netProc, &QProcess::readyReadStandardError,
            this, &ProgrammazionePage::onNetReadyReadStderr);
    connect(m_netProc,
            QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, &ProgrammazionePage::onNetFinished);
    m_netProc->start(m_netTool, args);
    if (!m_netProc->waitForStarted(2000)) {
        m_netStatus->setText(
            tr("\xe2\x9d\x8c  Impossibile avviare il tool. Permessi root necessari?"));
        LogBus::post("\xe2\x9d\x8c Programmazione: Impossibile avviare il tool di rete. Permessi root necessari?");
        return;
    }
    m_btnNetStart->setEnabled(false);
    m_btnNetStop->setEnabled(true);
    const QString filterDesc = filter.isEmpty() ? "tutto il traffico" : filter;
    m_netStatus->setText(
        QString("\xf0\x9f\x94\xb4  Cattura su <b>%1</b> — filtro: <code>%2</code>")
        .arg(iface, filterDesc));
}

void ProgrammazionePage::netStop()
{
    if (!m_netProc) return;
    m_netProc->terminate();
    QTimer::singleShot(1500, this, &ProgrammazionePage::onNetStopTimer);
}

void ProgrammazionePage::netAiAnalyze()
{
    const QString logText = m_netLog->toPlainText().trimmed();
    if (logText.isEmpty()) return;

    QStringList lines = logText.split('\n', Qt::SkipEmptyParts);
    if (lines.size() > 150) lines = lines.mid(lines.size() - 150);
    const QString snippet = lines.join('\n');

    const QString sys =
        "Sei un esperto di sicurezza di rete e analisi del traffico. "
        "Analizza il seguente dump di pacchetti e fornisci:\n"
        "1. Panoramica del traffico (protocolli dominanti, host attivi)\n"
        "2. Anomalie o comportamenti sospetti\n"
        "3. Potenziali problemi di sicurezza (porte inusuali, scansioni, flood)\n"
        "4. Suggerimenti pratici per approfondire o mitigare i rischi.\n"
        "Rispondi in italiano, in modo conciso e strutturato.";

    const QString user =
        QString("Ecco i pacchetti catturati:\n\n```\n%1\n```").arg(snippet);

    m_netAiOutput->clear();
    m_netAiOutput->setPlainText("\xf0\x9f\xa4\x96  Analisi in corso...\n\n");
    m_btnNetAnalyze->setEnabled(false);

    disconnect(m_netAiTokenConn);
    disconnect(m_netAiFinishedConn);
    disconnect(m_netAiErrorConn);
    m_netAiTokenConn    = connect(m_ai, &AiClient::token,    this, &ProgrammazionePage::onNetAiToken);
    m_netAiFinishedConn = connect(m_ai, &AiClient::finished, this, &ProgrammazionePage::onNetAiFinished);
    m_netAiErrorConn    = connect(m_ai, &AiClient::error,    this, &ProgrammazionePage::onNetAiError);
    m_ai->chat(P::prependKnowledge(sys), user);
}

/* ══════════════════════════════════════════════════════════════
   buildReteLan — sub-tab "🌐 Rete LAN"
   Scanner LAN: ARP cache, nmap ping-sweep, info interfacce,
   calcolo subnet (rete/broadcast/host) — nessun AI richiesto.
   ══════════════════════════════════════════════════════════════ */
QWidget* ProgrammazionePage::buildReteLan(QWidget* parent)
{
    auto* w   = new QWidget(parent);
    auto* lay = new QVBoxLayout(w);
    lay->setContentsMargins(12, 12, 12, 12);
    lay->setSpacing(8);

    /* ── Info interfacce locali ── */
    m_lanInfoLbl = new QLabel(w);
    m_lanInfoLbl->setWordWrap(true);
    m_lanInfoLbl->setTextFormat(Qt::RichText);
    m_lanInfoLbl->setObjectName("hintLabel");
    lanRefreshInfo();

    /* ── Pulsanti di scansione ── */
    auto* btnRow = new QWidget(w);
    auto* btnLay = new QHBoxLayout(btnRow);
    btnLay->setContentsMargins(0, 0, 0, 0);
    btnLay->setSpacing(8);

    m_lanScanArp  = new QPushButton(tr("\xf0\x9f\x94\x8d  ARP Cache (rapido)"), btnRow);
    m_lanScanNmap = new QPushButton(tr("\xf0\x9f\x8c\x90  Scan nmap (completo)"), btnRow);
    m_lanStopBtn  = new QPushButton(tr("\xe2\x8f\xb9  Stop"), btnRow);
    m_lanStopBtn->setObjectName("actionBtn");
    m_lanStopBtn->setProperty("danger", true);
    m_lanStopBtn->setEnabled(false);
    auto* btnRefresh = new QPushButton(tr("\xf0\x9f\x94\x84  Aggiorna info"), btnRow);

    btnLay->addWidget(m_lanScanArp);
    btnLay->addWidget(m_lanScanNmap);
    btnLay->addWidget(m_lanStopBtn);
    btnLay->addStretch();
    btnLay->addWidget(btnRefresh);

    /* ── Tabella risultati: IP | MAC | Hostname | Stato ── */
    m_lanTable = new QTableWidget(0, 4, w);
    m_lanTable->setHorizontalHeaderLabels({tr(" IP"), tr(" MAC Address"), tr(" Hostname"), tr(" Stato")});
    m_lanTable->setColumnWidth(0, 130);
    m_lanTable->setColumnWidth(1, 158);
    m_lanTable->setColumnWidth(3, 100);
    m_lanTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Fixed);
    m_lanTable->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Fixed);
    m_lanTable->horizontalHeader()->setSectionResizeMode(2, QHeaderView::Stretch);
    m_lanTable->horizontalHeader()->setSectionResizeMode(3, QHeaderView::Fixed);
    m_lanTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_lanTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_lanTable->setAlternatingRowColors(true);
    m_lanTable->verticalHeader()->setVisible(false);

    /* ── Status scan / info subnet ── */
    m_lanStatusLbl = new QLabel(w);
    m_lanStatusLbl->setObjectName("hintLabel");
    m_lanStatusLbl->setText(
        tr("\xf0\x9f\x94\x8c Premi un pulsante per scansionare la rete locale."));

    lay->addWidget(btnRow);
    lay->addWidget(m_lanTable, 1);
    lay->addWidget(m_lanStatusLbl);
    lay->addWidget(m_lanInfoLbl);

    /* Connessioni */
    connect(m_lanScanArp, &QPushButton::clicked,
            this, &ProgrammazionePage::onLanScanArpClicked);
    connect(m_lanScanNmap, &QPushButton::clicked,
            this, &ProgrammazionePage::onLanScanNmapClicked);
    connect(m_lanStopBtn, &QPushButton::clicked,
            this, &ProgrammazionePage::onLanStopBtnClicked);
    connect(btnRefresh, &QPushButton::clicked,
            this, &ProgrammazionePage::lanRefreshInfo);
    return w;
}

void ProgrammazionePage::lanRefreshInfo()
{
    if (!m_lanInfoLbl) return;

    auto isPhysical = [](const QString& n) {
        return n.startsWith("en") || n.startsWith("eth") ||
               n.startsWith("wl") || n.startsWith("em") || n.startsWith("ib");
    };

    struct IfRow { QString icon, name, ip, pfx, net, mac; bool physical; };
    QList<IfRow> rows;

    for (const QNetworkInterface& iface : QNetworkInterface::allInterfaces()) {
        if (iface.flags().testFlag(QNetworkInterface::IsLoopBack)) continue;
        if (!iface.flags().testFlag(QNetworkInterface::IsUp)) continue;
        const QString hw = iface.hardwareAddress().toUpper();
        for (const QNetworkAddressEntry& e : iface.addressEntries()) {
            if (e.ip().protocol() != QAbstractSocket::IPv4Protocol) continue;
            const quint32 ipRaw  = e.ip().toIPv4Address();
            const quint32 mskRaw = e.netmask().toIPv4Address();
            int pfx = 0; quint32 tmp = mskRaw;
            while (tmp) { pfx += (tmp >> 31) & 1; tmp <<= 1; }
            const bool phys = isPhysical(iface.name());
            const QString icon = iface.name().startsWith("wl")
                ? "\xf0\x9f\x93\xa1"   /* 📡 */
                : phys
                    ? "\xf0\x9f\x94\x8c"   /* 🔌 */
                    : "\xf0\x9f\x94\xa7";  /* 🔧 */
            rows.append({ icon, iface.name(), e.ip().toString(),
                          QString::number(pfx),
                          QHostAddress(ipRaw & mskRaw).toString(),
                          hw.isEmpty() ? "N/D" : hw, phys });
        }
    }

    if (rows.isEmpty()) {
        m_lanInfoLbl->setText(tr("<i>\xf0\x9f\x9f\xa1 Nessuna interfaccia IPv4 attiva</i>"));
        return;
    }

    /* Fisiche prima, poi virtuali */
    std::stable_partition(rows.begin(), rows.end(), [](const IfRow& r){ return r.physical; });

    QString html =
        "<table cellspacing='0' cellpadding='3' style='border-collapse:collapse'>"
        "<tr>"
        "<td style='color:#888;padding-right:16px'><small><b>Interfaccia</b></small></td>"
        "<td style='color:#888;padding-right:16px'><small><b>Indirizzo IP</b></small></td>"
        "<td style='color:#888;padding-right:16px'><small><b>Rete</b></small></td>"
        "<td style='color:#888'><small><b>MAC Address</b></small></td>"
        "</tr>";

    bool virtSepAdded = false;
    for (const IfRow& r : rows) {
        if (!r.physical && !virtSepAdded) {
            /* separatore prima delle virtuali */
            html += "<tr><td colspan='4' style='padding-top:4px;padding-bottom:2px'>"
                    "<small><span style='color:#555'>"
                    "\xe2\x94\x80\xe2\x94\x80"
                    " Virtuali / Bridge "
                    "\xe2\x94\x80\xe2\x94\x80"
                    "</span></small></td></tr>";
            virtSepAdded = true;
        }
        const QString style = r.physical
            ? "padding-right:16px"
            : "padding-right:16px;color:#777";
        const QString nameCell = r.physical
            ? QString("<b>%1 %2</b>").arg(r.icon, r.name)
            : QString("<small>%1 %2</small>").arg(r.icon, r.name);
        const QString dataStyle = r.physical ? "" : "color:#777";
        html += QString(
            "<tr>"
            "<td style='%1'>%2</td>"
            "<td style='%1'><small><code style='%3'>%4/%5</code></small></td>"
            "<td style='%1'><small><code style='%3'>%6</code></small></td>"
            "<td><small><code style='%3'>%7</code></small></td>"
            "</tr>")
            .arg(style, nameCell, dataStyle,
                 r.ip, r.pfx, r.net, r.mac);
    }
    html += "</table>";
    m_lanInfoLbl->setText(html);
}

/* ======================================================================
   Sezione 7 — Network Analyzer slots
   ====================================================================== */

void ProgrammazionePage::onBtnNetClearClicked()
{
    if (m_netLog) m_netLog->clear();
    if (m_netAiOutput) m_netAiOutput->clear();
    if (m_btnNetAnalyze) m_btnNetAnalyze->setEnabled(false);
}

void ProgrammazionePage::onNetReadyRead()
{
    if (!m_netProc) return;
    const QString out = QString::fromLocal8Bit(
        m_netProc->readAllStandardOutput());
    if (m_netLog) {
        m_netLog->moveCursor(QTextCursor::End);
        m_netLog->insertPlainText(out);
        m_netLog->ensureCursorVisible();
    }
    if (m_btnNetAnalyze) m_btnNetAnalyze->setEnabled(true);
}

void ProgrammazionePage::onNetReadyReadStderr()
{
    if (!m_netProc) return;
    const QString err = QString::fromLocal8Bit(m_netProc->readAllStandardError());
    if (err.isEmpty()) return;

    /* Permesso negato — CAP_NET_RAW mancante */
    const bool permErr = err.contains("permission", Qt::CaseInsensitive)
                      || err.contains("CAP_NET_RAW", Qt::CaseInsensitive)
                      || err.contains("packet socket", Qt::CaseInsensitive);
    if (permErr) {
        m_netProc->kill();
        if (m_btnNetStart)   m_btnNetStart->setEnabled(true);
        if (m_btnNetStop)    m_btnNetStop->setEnabled(false);
        const QString fix = QString("sudo setcap cap_net_raw+eip %1").arg(m_netTool);
        if (m_netStatus)
            m_netStatus->setText(
                "\xf0\x9f\x94\x91  Permessi insufficienti. "
                "Clicca \xe2\x80\x9c" "Fix permessi\xe2\x80\x9d"
                " oppure esegui nel terminale: <code>" + fix + "</code>");
        return;
    }

    /* Filtra messaggi informativi di tshark (interfaccia pronta) */
    if (err.contains("Capturing on", Qt::CaseInsensitive)) return;

    if (m_netLog) {
        m_netLog->moveCursor(QTextCursor::End);
        m_netLog->insertPlainText(QString("[stderr] %1").arg(err));
        m_netLog->ensureCursorVisible();
    }
}

void ProgrammazionePage::netFixPermissions()
{
    if (m_netTool.isEmpty()) return;
    const QString pkexec = QStandardPaths::findExecutable("pkexec");
    if (pkexec.isEmpty()) {
        if (m_netStatus)
            m_netStatus->setText(
                "\xe2\x9d\x8c  pkexec non trovato. Esegui manualmente: "
                "sudo setcap cap_net_raw+eip " + m_netTool);
        LogBus::post("\xe2\x9d\x8c Programmazione: pkexec non trovato.");
        return;
    }
    if (m_netStatus)
        m_netStatus->setText(tr("\xe2\x8f\xb3  Richiesta permessi in corso..."));
    auto* proc = new QProcess(this);
    /* context=this, proc è figlio di this → cattura sicura */
    connect(proc, QOverload<int,QProcess::ExitStatus>::of(&QProcess::finished),
            this, [this, proc](int code, QProcess::ExitStatus) {
                proc->deleteLater();
                if (m_netStatus)
                    m_netStatus->setText(code == 0
                        ? "\xe2\x9c\x85  Permessi applicati. Premi Start per avviare la cattura."
                        : "\xe2\x9d\x8c  Operazione annullata o fallita (code " +
                          QString::number(code) + ").");
                if (code != 0) LogBus::post(QString("\xe2\x9d\x8c Programmazione: setcap fallito (code %1).").arg(code));
            });
    proc->start(pkexec, {"setcap", "cap_net_raw+eip", m_netTool});
}

void ProgrammazionePage::onNetFinished(int code, QProcess::ExitStatus /*status*/)
{
    if (m_btnNetStart)  m_btnNetStart->setEnabled(true);
    if (m_btnNetStop)   m_btnNetStop->setEnabled(false);
    if (m_netStatus)
        m_netStatus->setText(
            code == 0
            ? "\xe2\x9c\x85  Cattura completata."
            : QString("\xe2\x9d\x8c  Terminato (code %1).").arg(code));
    if (code != 0) LogBus::post(QString("\xe2\x9d\x8c Programmazione: Network analyzer terminato (code %1).").arg(code));

    const bool hasData = m_netLog && !m_netLog->toPlainText().trimmed().isEmpty();
    if (m_btnNetAnalyze) m_btnNetAnalyze->setEnabled(hasData);
}

void ProgrammazionePage::onNetStopTimer()
{
    if (m_netProc && m_netProc->state() != QProcess::NotRunning)
        m_netProc->kill();
}

void ProgrammazionePage::onNetAiToken(const QString& tok)
{
    if (!m_netAiOutput) return;
    m_netAiOutput->moveCursor(QTextCursor::End);
    m_netAiOutput->insertPlainText(tok);
    m_netAiOutput->ensureCursorVisible();
}

void ProgrammazionePage::onNetAiFinished(const QString& /*full*/)
{
    disconnect(m_netAiTokenConn);
    disconnect(m_netAiFinishedConn);
    disconnect(m_netAiErrorConn);
    if (m_btnNetAnalyze) m_btnNetAnalyze->setEnabled(true);
}

void ProgrammazionePage::onNetAiError(const QString& msg)
{
    disconnect(m_netAiTokenConn);
    disconnect(m_netAiFinishedConn);
    disconnect(m_netAiErrorConn);
    if (m_btnNetAnalyze) m_btnNetAnalyze->setEnabled(true);
    if (m_netAiOutput) {
        m_netAiOutput->moveCursor(QTextCursor::End);
        m_netAiOutput->insertPlainText(
            QString("\n\xe2\x9d\x8c  Errore: %1").arg(msg));
    }
    LogBus::post("\xe2\x9d\x8c Programmazione: Network AI errore: " + msg);
}

/* ======================================================================
   Sezione 8 — Rete LAN slots
   ====================================================================== */

void ProgrammazionePage::lanAddRow(const QString& ip, const QString& mac,
                                    const QString& host, const QString& stato)
{
    if (!m_lanTable) return;
    const int row = m_lanTable->rowCount();
    m_lanTable->insertRow(row);
    m_lanTable->setItem(row, 0, new QTableWidgetItem(ip));
    m_lanTable->setItem(row, 1, new QTableWidgetItem(mac));
    m_lanTable->setItem(row, 2, new QTableWidgetItem(host));
    m_lanTable->setItem(row, 3, new QTableWidgetItem(stato));
}

void ProgrammazionePage::lanResetBtns()
{
    if (m_lanScanArp)  m_lanScanArp->setEnabled(true);
    if (m_lanScanNmap) m_lanScanNmap->setEnabled(true);
    if (m_lanStopBtn)  m_lanStopBtn->setEnabled(false);
}

void ProgrammazionePage::onLanScanArpClicked()
{
    if (m_lanTable) m_lanTable->setRowCount(0);
    if (m_lanStatusLbl) m_lanStatusLbl->setText(
        tr("\xf0\x9f\x94\x8d  Lettura ARP cache..."));
    if (m_lanScanArp)  m_lanScanArp->setEnabled(false);
    if (m_lanScanNmap) m_lanScanNmap->setEnabled(false);
    if (m_lanStopBtn)  m_lanStopBtn->setEnabled(true);

    if (m_lanProc && m_lanProc->state() != QProcess::NotRunning) {
        m_lanProc->kill();
        m_lanProc->waitForFinished(1000);
        m_lanProc->deleteLater();
        m_lanProc = nullptr;
    }
    m_lanBuf.clear();
    m_lanProc = new QProcess(this);

    connect(m_lanProc, &QProcess::errorOccurred,
            this, &ProgrammazionePage::onLanArpError);
    connect(m_lanProc, &QProcess::readyRead,
            this, &ProgrammazionePage::onLanArpReadyRead);
    connect(m_lanProc,
            QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, &ProgrammazionePage::onLanArpFinished);

    /* Usa `ip neigh show` — non richiede root né net-tools, legge la cache ARP del kernel */
    m_lanProc->start("ip", {"neigh", "show"});
}

void ProgrammazionePage::onLanScanNmapClicked()
{
    if (m_lanTable) m_lanTable->setRowCount(0);
    if (m_lanStatusLbl) m_lanStatusLbl->setText(
        tr("\xf0\x9f\x8c\x90  Avvio scansione nmap..."));
    if (m_lanScanArp)  m_lanScanArp->setEnabled(false);
    if (m_lanScanNmap) m_lanScanNmap->setEnabled(false);
    if (m_lanStopBtn)  m_lanStopBtn->setEnabled(true);

    /* Determina subnet dalla prima interfaccia attiva */
    QString subnet;
    for (const QNetworkInterface& iface : QNetworkInterface::allInterfaces()) {
        if (iface.flags().testFlag(QNetworkInterface::IsLoopBack)) continue;
        if (!iface.flags().testFlag(QNetworkInterface::IsUp)) continue;
        for (const QNetworkAddressEntry& e : iface.addressEntries()) {
            if (e.ip().protocol() != QAbstractSocket::IPv4Protocol) continue;
            const quint32 ipRaw  = e.ip().toIPv4Address();
            const quint32 mskRaw = e.netmask().toIPv4Address();
            int pfx = 0; quint32 tmp = mskRaw;
            while (tmp) { pfx += (tmp >> 31) & 1; tmp <<= 1; }
            const quint32 netRaw = ipRaw & mskRaw;
            subnet = QHostAddress(netRaw).toString() + "/" + QString::number(pfx);
            break;
        }
        if (!subnet.isEmpty()) break;
    }
    if (subnet.isEmpty()) {
        if (m_lanStatusLbl)
            m_lanStatusLbl->setText(
                tr("\xe2\x9d\x8c  Impossibile determinare la subnet."));
        LogBus::post("\xe2\x9d\x8c Programmazione: Impossibile determinare la subnet.");
        lanResetBtns();
        return;
    }
    m_lanNmapSubnet = subnet;

    if (m_lanProc && m_lanProc->state() != QProcess::NotRunning) {
        m_lanProc->kill();
        m_lanProc->waitForFinished(1000);
        m_lanProc->deleteLater();
        m_lanProc = nullptr;
    }
    m_lanBuf.clear();
    m_lanProc = new QProcess(this);

    connect(m_lanProc, &QProcess::errorOccurred,
            this, &ProgrammazionePage::onLanNmapError);
    connect(m_lanProc, &QProcess::readyRead,
            this, &ProgrammazionePage::onLanNmapReadyRead);
    connect(m_lanProc,
            QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, &ProgrammazionePage::onLanNmapFinished);

    if (m_lanStatusLbl)
        m_lanStatusLbl->setText(
            QString("\xf0\x9f\x8c\x90  nmap -sn %1 ...").arg(subnet));

    m_lanProc->start("nmap", {"-sn", subnet});
}

void ProgrammazionePage::onLanStopBtnClicked()
{
    if (m_lanProc && m_lanProc->state() != QProcess::NotRunning) {
        m_lanProc->terminate();
        QTimer::singleShot(1500, this, &ProgrammazionePage::onLanStopTimer);
    }
    lanResetBtns();
    if (m_lanStatusLbl) m_lanStatusLbl->setText(
        tr("\xe2\x8f\xb9  Scansione interrotta."));
}

void ProgrammazionePage::onLanArpError(QProcess::ProcessError err)
{
    Q_UNUSED(err)
    if (m_lanStatusLbl)
        m_lanStatusLbl->setText(
            "\xe2\x9d\x8c  Impossibile eseguire 'ip neigh show'. "
            "Installa iproute2: sudo apt install iproute2");
    LogBus::post("\xe2\x9d\x8c Programmazione: Impossibile eseguire 'ip neigh show'.");
    lanResetBtns();
    if (auto* p = qobject_cast<QProcess*>(sender())) { p->deleteLater(); m_lanProc = nullptr; }
}

void ProgrammazionePage::onLanArpReadyRead()
{
    if (!m_lanProc) return;
    m_lanBuf += QString::fromLocal8Bit(m_lanProc->readAll());
}

void ProgrammazionePage::onLanArpFinished(int /*code*/, QProcess::ExitStatus /*status*/)
{
    /* Parsing output `ip neigh show`:
       10.42.0.132 dev eno1 lladdr 34:15:9e:3b:f0:5c REACHABLE
       Ignora le righe FAILED (nessun lladdr). */
    const QStringList lines = m_lanBuf.split('\n', Qt::SkipEmptyParts);
    int count = 0;
    static const QRegularExpression reNeigh(
        R"((\d+\.\d+\.\d+\.\d+)\s+\S+\s+(\S+)\s+lladdr\s+([\da-fA-F:]{17}))");
    for (const QString& line : lines) {
        const auto m = reNeigh.match(line.trimmed());
        if (!m.hasMatch()) continue;
        lanAddRow(m.captured(1), m.captured(3).toUpper(), m.captured(2), "cached");
        ++count;
    }
    if (m_lanStatusLbl)
        m_lanStatusLbl->setText(
            count == 0
            ? "\xf0\x9f\x9f\xa1  Cache ARP vuota (nessun host raggiunto di recente)."
            : QString("\xe2\x9c\x85  %1 host trovati nella cache ARP.").arg(count));
    lanResetBtns();
    if (auto* p = qobject_cast<QProcess*>(sender())) { p->deleteLater(); m_lanProc = nullptr; }
}

void ProgrammazionePage::onLanNmapError(QProcess::ProcessError err)
{
    Q_UNUSED(err)
    if (m_lanStatusLbl)
        m_lanStatusLbl->setText(
            "\xe2\x9d\x8c  Impossibile eseguire 'nmap'. "
            "Installa nmap: sudo apt install nmap");
    lanResetBtns();
    if (auto* p = qobject_cast<QProcess*>(sender())) { p->deleteLater(); m_lanProc = nullptr; }
}

void ProgrammazionePage::onLanNmapReadyRead()
{
    if (!m_lanProc) return;
    m_lanBuf += QString::fromLocal8Bit(m_lanProc->readAll());
    /* Aggiorna status con numero righe ricevute */
    const int lines = m_lanBuf.count('\n');
    if (m_lanStatusLbl && lines % 10 == 0)
        m_lanStatusLbl->setText(
            QString("\xf0\x9f\x8c\x90  nmap -sn %1 ... (%2 righe)")
            .arg(m_lanNmapSubnet).arg(lines));
}

void ProgrammazionePage::onLanNmapFinished(int /*code*/, QProcess::ExitStatus /*status*/)
{
    /* Parsing output nmap -sn — ordine reale delle righe per host:
         Nmap scan report for [hostname (]ip[)]
         Host is up (latency).
         MAC Address: AA:BB:CC:DD:EE:FF (Vendor)   ← solo con root
       Bufferizziamo ip/host/mac per ogni host e le emettiamo al blocco successivo
       (o alla fine), così il MAC viene letto prima di chiamare lanAddRow. */
    const QStringList lines = m_lanBuf.split('\n', Qt::SkipEmptyParts);
    int count = 0;
    QString ip, host, mac;
    bool hostUp = false;

    static const QRegularExpression reIP(R"((\d+\.\d+\.\d+\.\d+))");
    static const QRegularExpression reHost(R"(^([^\s(]+))");
    static const QRegularExpression reMac(R"(MAC Address:\s+([\da-fA-F:]{17}))");

    auto flushHost = [&]() {
        if (ip.isEmpty() || !hostUp) return;
        lanAddRow(ip, mac, host, "online");
        ++count;
    };

    for (const QString& line : lines) {
        const QString l = line.trimmed();
        if (l.startsWith("Nmap scan report for ")) {
            flushHost();
            ip.clear(); host.clear(); mac.clear(); hostUp = false;
            const QString rest = l.mid(21);
            const auto m1 = reIP.match(rest);
            if (m1.hasMatch()) ip = m1.captured(1);
            const auto m2 = reHost.match(rest);
            if (m2.hasMatch() && m2.captured(1) != ip) host = m2.captured(1);
        } else if (l.startsWith("MAC Address:")) {
            const auto mm = reMac.match(l);
            if (mm.hasMatch()) mac = mm.captured(1);
        } else if (l.startsWith("Host is up")) {
            hostUp = true;
        }
    }
    flushHost(); // ultimo host del buffer

    /* Senza root nmap non legge i MAC — arricchisce dalla cache ARP del kernel */
    enrichMacFromNeigh();

    if (m_lanStatusLbl)
        m_lanStatusLbl->setText(
            count == 0
            ? "\xf0\x9f\x9f\xa1  Nessun host trovato."
            : QString("\xe2\x9c\x85  %1 host online trovati con nmap.").arg(count));
    lanResetBtns();
    if (auto* p = qobject_cast<QProcess*>(sender())) { p->deleteLater(); m_lanProc = nullptr; }
}

void ProgrammazionePage::enrichMacFromNeigh()
{
    if (!m_lanTable || m_lanTable->rowCount() == 0) return;

    QMap<QString, QString> cache;

    /* 1. Cache ARP del kernel — host remoti già raggiunti */
    const QString arpOut = ProcHelper::readOutput("ip", {"neigh", "show"}, 2000);
    if (!arpOut.isEmpty()) {
        static const QRegularExpression reNeigh(
            R"((\d+\.\d+\.\d+\.\d+)\s+\S+\s+\S+\s+lladdr\s+([\da-fA-F:]{17}))");
        QRegularExpressionMatchIterator it = reNeigh.globalMatch(arpOut);
        while (it.hasNext()) {
            const auto m = it.next();
            cache[m.captured(1)] = m.captured(2).toUpper();
        }
    }

    /* 2. IP locali del PC — non compaiono in ip neigh (non ci si fa ARP con se stessi).
          Costruiamo una mappa ip→MAC dalle interfacce di sistema. */
    for (const QNetworkInterface& iface : QNetworkInterface::allInterfaces()) {
        const QString hw = iface.hardwareAddress().toUpper();
        if (hw.isEmpty()) continue;
        for (const QNetworkAddressEntry& e : iface.addressEntries()) {
            if (e.ip().protocol() != QAbstractSocket::IPv4Protocol) continue;
            cache[e.ip().toString()] = hw;
        }
    }

    for (int r = 0; r < m_lanTable->rowCount(); ++r) {
        auto* macItem = m_lanTable->item(r, 1);
        if (!macItem || !macItem->text().isEmpty()) continue;
        auto* ipItem = m_lanTable->item(r, 0);
        if (!ipItem) continue;
        const QString mac = cache.value(ipItem->text());
        if (!mac.isEmpty())
            macItem->setText(mac);
    }
}

void ProgrammazionePage::onLanStopTimer()
{
    if (m_lanProc && m_lanProc->state() != QProcess::NotRunning)
        m_lanProc->kill();
}
