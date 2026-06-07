/* ======================================================================
   main_net_subnet.cpp — Calcolatore sottoreti con grafo Graphviz

   Sub-tab "🔢 Sottoreti" di Rete & Network in ProgrammazionePage.
   - Input CIDR (es. 192.168.0.0/24) + numero di subnet desiderate
   - Calcolo VLSM in C++ puro (QHostAddress + aritmetica IPv4)
   - Tabella risultati: rete, maschera, primo/ultimo host, n. host
   - Grafo Graphviz dark-theme renderizzato con dot -Tpng
   ====================================================================== */
#include "main_programming.h"
#include "../log_bus.h"
#include "../dpi_utils.h"

#include <QComboBox>
#include <QDir>
#include <QFile>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QHostAddress>
#include <QLabel>
#include <QLineEdit>
#include <QProcess>
#include <QPushButton>
#include <QScrollArea>
#include <QSpinBox>
#include <QSplitter>
#include <QTextEdit>
#include <QTextStream>
#include <QVBoxLayout>
#include <cmath>

/* ── Struttura dati per una singola sottorete ─────────────────────── */
struct SubnetInfo {
    quint32 network;    // indirizzo di rete (host byte order)
    quint32 broadcast;  // broadcast
    quint32 mask;       // maschera
    int     prefix;     // lunghezza prefisso /N
    int     hostCount;  // host utilizzabili

    QString networkStr()   const { return QHostAddress(network).toString();   }
    QString broadcastStr() const { return QHostAddress(broadcast).toString();  }
    QString maskStr()      const { return QHostAddress(mask).toString();       }
    QString firstHostStr() const {
        return (hostCount > 0) ? QHostAddress(network + 1).toString()
                               : QStringLiteral("N/A");
    }
    QString lastHostStr()  const {
        return (hostCount > 0) ? QHostAddress(broadcast - 1).toString()
                               : QStringLiteral("N/A");
    }
    QString cidr() const {
        return networkStr() + "/" + QString::number(prefix);
    }
};

/* ── Calcola le subnet di una rete padre ───────────────────────────── */
static QVector<SubnetInfo> calcSubnets(quint32 parentNet, int parentPrefix,
                                       int nSubnets)
{
    QVector<SubnetInfo> result;
    if (nSubnets <= 0 || parentPrefix > 30) return result;

    /* Quanti bit aggiuntivi servono per contenere nSubnets subnet? */
    const int subBits = (nSubnets <= 1)
        ? 0
        : static_cast<int>(std::ceil(std::log2(static_cast<double>(nSubnets))));

    const int newPrefix = parentPrefix + subBits;
    if (newPrefix > 30) return result;  /* niente spazio per host */

    const quint32 newMask     = (newPrefix == 0) ? 0u : (~0u << (32 - newPrefix));
    const quint32 subnetSize  = 1u << (32 - newPrefix);  /* indirizzi per subnet */
    const int     usableHosts = static_cast<int>(subnetSize) - 2;

    for (int i = 0; i < nSubnets; ++i) {
        SubnetInfo s;
        s.network   = (parentNet & newMask) + static_cast<quint32>(i) * subnetSize;
        s.broadcast = s.network + subnetSize - 1;
        s.mask      = newMask;
        s.prefix    = newPrefix;
        s.hostCount = usableHosts;
        result.append(s);
    }
    return result;
}

/* ── Genera il codice DOT Graphviz dark-theme ──────────────────────── */
static QString buildDot(const QString& parentCidr, quint32 parentNet,
                         int parentPrefix, const QVector<SubnetInfo>& subnets)
{
    const quint32 parentMask =
        (parentPrefix == 0) ? 0u : (~0u << (32 - parentPrefix));
    const int parentHosts =
        (parentPrefix >= 30) ? 2 : (1 << (32 - parentPrefix)) - 2;

    QString dot;
    QTextStream ts(&dot);
    ts << "digraph subnets {\n"
       << "  graph [bgcolor=\"#0f172a\" fontname=\"monospace\" fontsize=11"
       << " fontcolor=\"#94a3b8\" rankdir=TB"
       << " label=\"" << parentCidr << "  \xe2\x86\x92  "
       << subnets.size() << " subnet\"]\n"
       << "  node [style=\"filled,rounded\" fontname=\"monospace\" fontsize=10"
       << " fontcolor=\"#e2e8f0\" margin=0.15]\n"
       << "  edge [color=\"#475569\" arrowsize=0.7 penwidth=1.5]\n\n";

    /* Nodo padre */
    ts << "  P [label=\"" << parentCidr
       << "\\nMaschera: " << QHostAddress(parentMask).toString()
       << "\\nHost totali: " << parentHosts
       << "\" shape=house fillcolor=\"#1d4ed8\" color=\"#3b82f6\"]\n\n";

    /* Nodi subnet */
    const QStringList colors = {
        "#0f766e","#15803d","#b45309","#7c3aed",
        "#0369a1","#be185d","#9a3412","#065f46",
    };
    for (int i = 0; i < subnets.size(); ++i) {
        const SubnetInfo& s = subnets[i];
        const QString color = colors[i % colors.size()];
        ts << "  N" << i
           << " [label=\"" << s.cidr()
           << "\\nMask: " << s.maskStr()
           << "\\n" << s.firstHostStr() << " \xe2\x80\x94 " << s.lastHostStr()
           << "\\n" << s.hostCount << " host utilizzabili"
           << "\" shape=box fillcolor=\"" << color << "\" color=\"#64748b\"]\n";
    }

    ts << "\n";
    for (int i = 0; i < subnets.size(); ++i)
        ts << "  P -> N" << i << "\n";

    ts << "}\n";
    return dot;
}

/* ──────────────────────────────────────────────────────────────────────
   buildSubnetTab — costruisce il sub-tab Subnet Calculator
   ────────────────────────────────────────────────────────────────────── */
QWidget* ProgrammazionePage::buildSubnetTab(QWidget* parent)
{
    auto* w   = new QWidget(parent);
    auto* lay = new QVBoxLayout(w);
    lay->setContentsMargins(12, 12, 12, 12);
    lay->setSpacing(8);

    /* ── Riga input ── */
    auto* inputRow = new QWidget(w);
    auto* inputHL  = new QHBoxLayout(inputRow);
    inputHL->setContentsMargins(0, 0, 0, 0);
    inputHL->setSpacing(8);

    auto* cidrLbl = new QLabel("CIDR:", inputRow);
    m_subnetInput = new QLineEdit(inputRow);
    m_subnetInput->setPlaceholderText("192.168.0.0/24");
    m_subnetInput->setFixedWidth(dpiScale(180));
    m_subnetInput->setText("192.168.0.0/24");
    m_subnetInput->setFont(QFont("JetBrains Mono,Fira Code,Consolas,Monospace", 10));

    auto* nLbl    = new QLabel(tr("N. subnet:"), inputRow);
    m_subnetCount = new QSpinBox(inputRow);
    m_subnetCount->setRange(2, 64);
    m_subnetCount->setValue(4);
    m_subnetCount->setFixedWidth(dpiScale(70));

    auto* btnCalc = new QPushButton("\xf0\x9f\x94\xa2  Calcola", inputRow);
    btnCalc->setObjectName("actionBtn");
    auto* btnDot  = new QPushButton("\xf0\x9f\x95\xb8  Aggiorna grafo", inputRow);

    m_subnetStatusLbl = new QLabel(inputRow);
    m_subnetStatusLbl->setObjectName("hintLabel");

    inputHL->addWidget(cidrLbl);
    inputHL->addWidget(m_subnetInput);
    inputHL->addSpacing(12);
    inputHL->addWidget(nLbl);
    inputHL->addWidget(m_subnetCount);
    inputHL->addSpacing(12);
    inputHL->addWidget(btnCalc);
    inputHL->addWidget(btnDot);
    inputHL->addStretch();
    inputHL->addWidget(m_subnetStatusLbl);
    lay->addWidget(inputRow);

    /* ── Splitter: risultati testo | grafo ── */
    auto* splitter = new QSplitter(Qt::Horizontal, w);
    splitter->setHandleWidth(6);

    /* Pannello sinistro — tabella testuale */
    auto* leftPanel = new QGroupBox(
        "\xf0\x9f\x93\x8a  Risultati subnet", splitter);
    auto* leftLay   = new QVBoxLayout(leftPanel);
    m_subnetResults = new QTextEdit(leftPanel);
    m_subnetResults->setReadOnly(true);
    m_subnetResults->setFont(QFont("JetBrains Mono,Fira Code,Consolas,Monospace", 9));
    m_subnetResults->setPlaceholderText(
        tr("Inserisci un CIDR e clicca Calcola..."));
    leftLay->addWidget(m_subnetResults);

    /* Pannello destro — immagine Graphviz */
    auto* rightPanel = new QGroupBox(
        "\xf0\x9f\x95\xb8  Grafo subnet (Graphviz)", splitter);
    auto* rightLay   = new QVBoxLayout(rightPanel);

    auto* scrollArea = new QScrollArea(rightPanel);
    scrollArea->setWidgetResizable(true);
    scrollArea->setFrameShape(QFrame::NoFrame);

    m_subnetGraphImg = new QLabel(scrollArea);
    m_subnetGraphImg->setAlignment(Qt::AlignCenter);
    m_subnetGraphImg->setMinimumSize(dpiScale(200), dpiScale(200));
    m_subnetGraphImg->setText(
        "\xf0\x9f\x95\xb8  Il grafo apparir\xc3\xa0 dopo il calcolo");
    m_subnetGraphImg->setObjectName("hintLabel");
    m_subnetGraphImg->setWordWrap(true);
    scrollArea->setWidget(m_subnetGraphImg);
    rightLay->addWidget(scrollArea);

    splitter->addWidget(leftPanel);
    splitter->addWidget(rightPanel);
    splitter->setStretchFactor(0, 2);
    splitter->setStretchFactor(1, 3);
    lay->addWidget(splitter, 1);

    /* ── Connessioni ── */
    connect(btnCalc, &QPushButton::clicked,
            this, &ProgrammazionePage::onSubnetCalculateClicked);

    connect(btnDot, &QPushButton::clicked, this,
            [this]() { onSubnetRenderGraph(); });

    /* Enter nel campo CIDR avvia il calcolo */
    connect(m_subnetInput, &QLineEdit::returnPressed,
            this, &ProgrammazionePage::onSubnetCalculateClicked);

    return w;
}

/* ──────────────────────────────────────────────────────────────────────
   onSubnetCalculateClicked — calcola le subnet e aggiorna i risultati
   ────────────────────────────────────────────────────────────────────── */
void ProgrammazionePage::onSubnetCalculateClicked()
{
    if (!m_subnetInput || !m_subnetResults || !m_subnetStatusLbl) return;

    const QString cidrStr = m_subnetInput->text().trimmed();
    const int nSubnets    = m_subnetCount ? m_subnetCount->value() : 4;

    /* Parsing CIDR */
    const int slash = cidrStr.indexOf('/');
    if (slash < 0) {
        m_subnetStatusLbl->setText(tr("\xe2\x9d\x8c  Formato: 192.168.0.0/24"));
        return;
    }
    const QString ipStr = cidrStr.left(slash);
    bool prefixOk = false;
    const int prefix = cidrStr.mid(slash + 1).toInt(&prefixOk);

    if (!prefixOk || prefix < 1 || prefix > 30) {
        m_subnetStatusLbl->setText(tr("\xe2\x9d\x8c  Prefisso /1 \xe2\x80\x93 /30"));
        return;
    }

    QHostAddress ipAddr(ipStr);
    if (ipAddr.isNull() || ipAddr.protocol() != QAbstractSocket::IPv4Protocol) {
        m_subnetStatusLbl->setText(tr("\xe2\x9d\x8c  Indirizzo IPv4 non valido"));
        return;
    }

    const quint32 rawIp     = ipAddr.toIPv4Address();
    const quint32 parentMask = (~0u << (32 - prefix));
    const quint32 parentNet  = rawIp & parentMask;

    /* Calcola subnet */
    const QVector<SubnetInfo> subnets = calcSubnets(parentNet, prefix, nSubnets);

    if (subnets.isEmpty()) {
        m_subnetStatusLbl->setText(
            tr("\xe2\x9d\x8c  Impossibile suddividere /%1 in %2 subnet")
            .arg(prefix).arg(nSubnets));
        LogBus::post(
            QString("\xe2\x9d\x8c Subnet: impossibile suddividere /%1 in %2 subnet")
            .arg(prefix).arg(nSubnets));
        return;
    }

    /* ── Componi testo risultati ── */
    const int newPrefix  = subnets.first().prefix;
    const int newHosts   = subnets.first().hostCount;
    const int subnetBits = newPrefix - prefix;

    QString txt;
    QTextStream ts(&txt);
    ts << "Rete padre : " << QHostAddress(parentNet).toString()
       << "/" << prefix << "\n";
    ts << "Maschera   : " << QHostAddress(parentMask).toString() << "\n";
    ts << "Subnet     : " << subnets.size()
       << "  (+" << subnetBits << " bit, /" << newPrefix << ")\n";
    ts << "Host/subnet: " << newHosts << " utilizzabili\n";
    ts << QString(50, '-') << "\n\n";

    const int colW = 18;
    ts << QString("%-*1$s %-*1$s %-*1$s %-*1$s %s\n")
              .arg(colW)
         .arg("Rete / CIDR").arg("Maschera").arg("Primo host").arg("Ultimo host")
         .arg("Host #");

    /* Intestazione tabella */
    ts << qSetFieldWidth(colW) << Qt::left << "Rete / CIDR"
       << qSetFieldWidth(colW) << "Maschera"
       << qSetFieldWidth(colW) << "Primo host"
       << qSetFieldWidth(colW) << "Ultimo host"
       << qSetFieldWidth(0)    << "Host #\n";
    ts << QString(colW * 4 + 6, '-') << "\n";

    for (int i = 0; i < subnets.size(); ++i) {
        const SubnetInfo& s = subnets[i];
        ts << qSetFieldWidth(colW) << Qt::left << s.cidr()
           << qSetFieldWidth(colW) << s.maskStr()
           << qSetFieldWidth(colW) << s.firstHostStr()
           << qSetFieldWidth(colW) << s.lastHostStr()
           << qSetFieldWidth(0)    << s.hostCount << "\n";
    }

    ts << "\n" << QString(50, '-') << "\n";
    ts << "Broadcast subnet 0 : " << subnets.first().broadcastStr() << "\n";
    if (subnets.size() > 1)
        ts << "Broadcast subnet N : " << subnets.last().broadcastStr() << "\n";

    m_subnetResults->setPlainText(txt);
    m_subnetStatusLbl->setText(
        QString("\xe2\x9c\x85  %1 subnet calcolate").arg(subnets.size()));
    LogBus::post(
        QString("\xf0\x9f\x94\xa2 Subnet: %1 \xe2\x86\x92 %2 subnet /%3")
        .arg(cidrStr).arg(subnets.size()).arg(newPrefix));

    /* Salva per renderizzare il grafo */
    m_subnetLastDot = buildDot(cidrStr, parentNet, prefix, subnets);
    onSubnetRenderGraph();
}

/* ──────────────────────────────────────────────────────────────────────
   onSubnetRenderGraph — chiama dot -Tpng sul DOT salvato
   ────────────────────────────────────────────────────────────────────── */
void ProgrammazionePage::onSubnetRenderGraph()
{
    if (m_subnetLastDot.isEmpty()) return;
    if (m_subnetDotProc && m_subnetDotProc->state() != QProcess::NotRunning)
        m_subnetDotProc->kill();

    const QString tmpDot = QDir::tempPath() + "/prismalux_subnet.dot";
    m_subnetTmpPng       = QDir::tempPath() + "/prismalux_subnet.png";

    {
        QFile f(tmpDot);
        if (f.open(QFile::WriteOnly | QFile::Text))
            QTextStream(&f) << m_subnetLastDot;
    }

    if (m_subnetStatusLbl)
        m_subnetStatusLbl->setText(tr("\xe2\x8f\xb3  Rendering grafo..."));

    m_subnetDotProc = new QProcess(this);
    connect(m_subnetDotProc,
            QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, &ProgrammazionePage::onSubnetDotFinished);
    m_subnetDotProc->start("dot", {"-Tpng", tmpDot, "-o", m_subnetTmpPng});

    if (!m_subnetDotProc->waitForStarted(3000)) {
        if (m_subnetStatusLbl)
            m_subnetStatusLbl->setText(
                tr("\xe2\x9d\x8c  Graphviz non trovato — sudo apt install graphviz"));
        LogBus::post("\xe2\x9d\x8c Subnet: Graphviz (dot) non trovato nel PATH");
        m_subnetDotProc->deleteLater();
        m_subnetDotProc = nullptr;
    }
}

/* ──────────────────────────────────────────────────────────────────────
   onSubnetDotFinished — carica il PNG nel QLabel
   ────────────────────────────────────────────────────────────────────── */
void ProgrammazionePage::onSubnetDotFinished(int code, QProcess::ExitStatus)
{
    if (!m_subnetGraphImg) return;
    m_subnetDotProc->deleteLater();
    m_subnetDotProc = nullptr;

    if (code != 0) {
        m_subnetGraphImg->setText(
            "\xe2\x9d\x8c  Graphviz ha restituito un errore. "
            "Controlla che 'dot' sia nel PATH.");
        if (m_subnetStatusLbl)
            m_subnetStatusLbl->setText(tr("\xe2\x9d\x8c  Errore Graphviz"));
        LogBus::post("\xe2\x9d\x8c Subnet: Graphviz ha restituito errore");
        return;
    }

    const QPixmap px(m_subnetTmpPng);
    if (px.isNull()) {
        m_subnetGraphImg->setText("\xe2\x9d\x8c  PNG non valido");
        return;
    }

    m_subnetGraphImg->setPixmap(
        px.scaled(m_subnetGraphImg->width(),
                  m_subnetGraphImg->height() > 0
                      ? m_subnetGraphImg->height() : 600,
                  Qt::KeepAspectRatio,
                  Qt::SmoothTransformation));

    if (m_subnetStatusLbl)
        m_subnetStatusLbl->setText(tr("\xe2\x9c\x85  Grafo aggiornato"));
}
