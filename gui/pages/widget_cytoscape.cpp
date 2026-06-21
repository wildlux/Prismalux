#include "widget_cytoscape.h"
#include "../dpi_utils.h"
#include "../prismalux_paths.h"
#include "../widgets/model_combo_box.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QLineEdit>
#include <QTextEdit>
#include <QComboBox>
#include <QProcess>
#include <QTcpSocket>
#include <QTimer>
#include <QProgressBar>
#include <QFrame>
namespace P = PrismaluxPaths;

static const char* kSys[] = {
    "Sei un esperto di Cytoscape e py2cytoscape. "
    "Genera SOLO codice Python che usa la CyREST API (localhost:1234). "
    "import requests; BASE='http://localhost:1234/v1'. "
    "Usa endpoints /networks, /styles, /layouts, /commands. "
    "Rispondi SOLO con il blocco codice Python tra ``` e ```.",
    "Sei un esperto di bioinformatica e Cytoscape. "
    "Genera SOLO codice Python CyREST per creare un grafo di rete proteica/biologica. "
    "Includi nodi (proteine/geni) e archi (interazioni). Rispondi SOLO con codice Python tra ``` e ```.",
    "Sei un esperto di analisi reti e Cytoscape. "
    "Genera SOLO codice Python CyREST per calcolare centralit\xc3\xa0, clustering, componenti connesse. "
    "Rispondi SOLO con il blocco codice Python tra ``` e ```.",
    "Sei un esperto di Cytoscape. "
    "Genera SOLO codice Python CyREST per applicare un layout (force-directed, hierarchical, ecc.) "
    "e cambiare lo stile visivo (colori, dimensioni nodi). Rispondi SOLO con codice Python tra ``` e ```.",
    "Sei un esperto di Cytoscape. "
    "Genera SOLO codice Python CyREST libero. "
    "Rispondi SOLO con il blocco codice Python tra ``` e ```.",
    nullptr
};
static const char* kActions[] = {
    "\xf0\x9f\x94\xac  Nuova rete",
    "\xf0\x9f\xa7\xac  Rete biologica/PPI",
    "\xf0\x9f\x93\x88  Analisi centralit\xc3\xa0",
    "\xf0\x9f\x8e\xa8  Layout & stile",
    "\xf0\x9f\x90\x8d  Script libero",
    nullptr
};

CytoscapeWidget::CytoscapeWidget(AiClient* ai, QWidget* parent)
    : AiScriptWidget(ai, parent)
{
    auto* lay = new QVBoxLayout(this);
    lay->setContentsMargins(8, 8, 8, 8);
    lay->setSpacing(6);

    auto* descLbl = new QLabel(
        "\xf0\x9f\x94\xac  <i>Cytoscape \xe2\x80\x94 Piattaforma open-source per visualizzazione "
        "e analisi di reti biologiche e molecolari.</i>", this);
    descLbl->setObjectName("hintLabel");
    descLbl->setTextFormat(Qt::RichText);
    descLbl->setWordWrap(true);
    lay->addWidget(descLbl);

    /* ── Riga connessione ── */
    auto* connRow = new QWidget(this);
    auto* connLay = new QHBoxLayout(connRow);
    connLay->setContentsMargins(0, 0, 0, 0);
    connLay->setSpacing(8);
    auto* lbl = new QLabel("CyREST:", connRow);
    lbl->setObjectName("hintLabel");
    m_hostEdit = new QLineEdit("localhost:1234", connRow);
    m_hostEdit->setFixedWidth(dpiScale(150));
    auto* pingBtn = new QPushButton("\xf0\x9f\x94\x97  Verifica", connRow);
    pingBtn->setObjectName("actionBtn");
    pingBtn->setFixedWidth(dpiScale(100));
    m_statusLbl = new QLabel("\xe2\x9a\xaa  Non connesso", connRow);
    m_statusLbl->setObjectName("hintLabel");
    m_execBtn = new QPushButton("\xf0\x9f\x94\xac  Esegui su Cytoscape", connRow);
    m_execBtn->setObjectName("actionBtn");
    m_execBtn->setFixedWidth(dpiScale(180));
    m_execBtn->setEnabled(false);
    connLay->addWidget(lbl);
    connLay->addWidget(m_hostEdit);
    connLay->addWidget(pingBtn);
    connLay->addWidget(m_statusLbl, 1);
    connLay->addWidget(m_execBtn);
    lay->addWidget(connRow);

    auto* hintLbl = new QLabel(
        "\xf0\x9f\x94\xac <b>Cytoscape MCP:</b> analisi reti biologiche e sociali. "
        "Avvia Cytoscape (abilita CyREST su porta 1234).<br>"
        "Installa: <code>pip install py2cytoscape requests</code>", this);
    hintLbl->setObjectName("hintLabel");
    hintLbl->setWordWrap(true);
    lay->addWidget(hintLbl);

    /* ── Riga azione/modello ── */
    auto* toolRow = new QWidget(this);
    auto* toolLay = new QHBoxLayout(toolRow);
    toolLay->setContentsMargins(0, 0, 0, 0);
    toolLay->setSpacing(8);
    m_action = new QComboBox(toolRow);
    for (int i = 0; kActions[i]; i++)
        m_action->addItem(QString::fromUtf8(kActions[i]));
    m_model = new ModelComboBox(m_ai, toolRow);
    toolLay->addWidget(new QLabel("Analisi:", toolRow));
    toolLay->addWidget(m_action, 1);
    toolLay->addWidget(new QLabel("Modello:", toolRow));
    toolLay->addWidget(m_model, 1);
    lay->addWidget(toolRow);

    m_input = new QTextEdit(this);
    m_input->setPlaceholderText(
        "Descrivi la rete da analizzare...\n"
        "Es: 'Crea un grafo di interazione proteica con 10 proteine e mostra i hub'");
    m_input->setFixedHeight(dpiScale(80));
    lay->addWidget(m_input);

    auto* btnRow = new QWidget(this);
    auto* btnLay = new QHBoxLayout(btnRow);
    btnLay->setContentsMargins(0, 0, 0, 0);
    m_runBtn  = new QPushButton("\xf0\x9f\xa4\x96  Genera script Cytoscape", btnRow);
    m_runBtn->setObjectName("actionBtn");
    m_stopBtn = new QPushButton("\xe2\x8f\xb9  Stop", btnRow);
    m_stopBtn->setObjectName("actionBtn");
    m_stopBtn->setProperty("danger", true);
    m_stopBtn->setEnabled(false);
    btnLay->addWidget(m_runBtn);
    btnLay->addWidget(m_stopBtn);
    btnLay->addStretch();
    lay->addWidget(btnRow);

    m_output = new QTextEdit(this);
    m_output->setReadOnly(true);
    m_output->setObjectName("outputView");
    m_output->setPlaceholderText(tr("Script Python CyREST appare qui..."));
    lay->addWidget(m_output, 1);

    connect(pingBtn,   &QPushButton::clicked, this, &CytoscapeWidget::onPingClicked);
    connect(m_execBtn, &QPushButton::clicked, this, &CytoscapeWidget::onExecClicked);
    connect(m_runBtn,  &QPushButton::clicked, this, &CytoscapeWidget::onRunClicked);
    connect(m_stopBtn, &QPushButton::clicked, this, &CytoscapeWidget::onStopClicked);
}

void CytoscapeWidget::onPingClicked()
{
    const QString addr = m_hostEdit->text().trimmed();
    const QString host = addr.contains(':') ? addr.section(':', 0, 0) : addr;
    const int port = addr.contains(':') ? addr.section(':', 1).toInt() : 1234;
    m_statusLbl->setText(tr("\xf0\x9f\x94\x84  Connessione..."));
    if (m_sock) { m_sock->abort(); m_sock->deleteLater(); m_sock = nullptr; }
    m_sock = new QTcpSocket(this);
    m_sock->connectToHost(host, static_cast<quint16>(port));
    connect(m_sock, &QTcpSocket::connected,              this, &CytoscapeWidget::onSockConnected);
    connect(m_sock, &QAbstractSocket::errorOccurred,     this, &CytoscapeWidget::onSockError);
    QTimer::singleShot(3000, this, &CytoscapeWidget::onPingTimeout);
}

void CytoscapeWidget::onSockConnected()
{
    auto* sock = qobject_cast<QTcpSocket*>(sender());
    if (sock) { sock->disconnectFromHost(); sock->deleteLater(); }
    if (m_sock == sock) m_sock = nullptr;
    m_statusLbl->setText(tr("\xe2\x9c\x85  Server raggiungibile"));
    m_execBtn->setEnabled(!m_code.isEmpty());
}

void CytoscapeWidget::onSockError(QAbstractSocket::SocketError)
{
    auto* sock = qobject_cast<QTcpSocket*>(sender());
    m_statusLbl->setText("\xe2\x9d\x8c  " + (sock ? sock->errorString() : QString()));
    if (sock) sock->deleteLater();
    if (m_sock == sock) m_sock = nullptr;
}

void CytoscapeWidget::onPingTimeout()
{
    if (m_sock && m_sock->state() != QAbstractSocket::ConnectedState) {
        m_statusLbl->setText(tr("\xe2\x9d\x8c  Timeout"));
        m_sock->abort();
        m_sock->deleteLater();
        m_sock = nullptr;
    }
}

void CytoscapeWidget::onExecClicked()
{
    runSciScript(m_code, false, m_statusLbl, m_execBtn, m_output, m_proc, this);
}

void CytoscapeWidget::onRunClicked()
{
    const int idx = m_action->currentIndex();
    if (idx < 0 || !kSys[idx]) return;
    avviaSci(QString::fromUtf8(kSys[idx]), m_input->toPlainText(),
             m_output, m_runBtn, m_stopBtn, m_model, m_execBtn, &m_code, m_statusLbl);
}

void CytoscapeWidget::onStopClicked()
{
    m_ai->abort();
    m_runBtn->setEnabled(true);
    m_stopBtn->setEnabled(false);
}
