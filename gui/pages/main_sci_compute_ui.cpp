/* ══════════════════════════════════════════════════════════════
   main_sci_compute_ui.cpp — Costruzione UI e refresh tabelle
   ══════════════════════════════════════════════════════════════ */
#include "main_sci_compute.h"
#include "../dpi_utils.h"
#include <QJsonDocument>
#include <QJsonArray>
#include <QDateTime>
#include <QGridLayout>
#include <QDir>
#include <QFile>

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QSplitter>
#include <QGroupBox>
#include <QTabWidget>
#include <QStackedWidget>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QComboBox>
#include <QCheckBox>
#include <QTableWidget>
#include <QHeaderView>
#include <QTextEdit>
#include <QScrollArea>
#include <QRadioButton>
#include <QButtonGroup>
#include <QDateTime>

/* Colore status → HTML */
static QString statusColor(const QString& s)
{
    if (s == "done" || s == "validated") return "#22c55e";
    if (s == "running")                  return "#3b82f6";
    if (s == "error" || s == "conflict") return "#ef4444";
    if (s == "pending")                  return "#f59e0b";
    return "#6b7280";
}

static QString statusIcon(const QString& s)
{
    if (s == "done" || s == "validated") return "\xe2\x9c\x85";
    if (s == "running")                  return "\xf0\x9f\x94\x84";
    if (s == "error")                    return "\xe2\x9d\x8c";
    if (s == "conflict")                 return "\xe2\x9a\xa0";
    if (s == "pending")                  return "\xe2\x8f\xb3";
    return "?";
}

/* ══════════════════════════════════════════════════════════════
   buildUi
   ══════════════════════════════════════════════════════════════ */
QWidget* SciComputePage::buildUi()
{
    auto* root    = new QWidget;
    auto* rootLay = new QVBoxLayout(root);
    rootLay->setContentsMargins(dpiScale(10), dpiScale(8), dpiScale(10), dpiScale(8));
    rootLay->setSpacing(dpiScale(8));

    /* ── Titolo ── */
    auto* titleLbl = new QLabel(
        "\xf0\x9f\x94\xac  <b>Calcolo Scientifico Distribuito</b>"
        "  <span style='color:gray;font-size:11px;'>"
        "BOINC-like — bioinformatica \xc2\xb7 fisica \xc2\xb7 chimica \xc2\xb7 medicina"
        "</span>", root);
    titleLbl->setTextFormat(Qt::RichText);
    titleLbl->setObjectName("pageHeader");
    rootLay->addWidget(titleLbl);

    /* ── Barra config: Coordinator | Worker ── */
    auto* cfgBar    = new QWidget(root);
    auto* cfgLay    = new QHBoxLayout(cfgBar);
    cfgLay->setContentsMargins(0,0,0,0); cfgLay->setSpacing(dpiScale(8));

    auto* coordRb = new QRadioButton("\xf0\x9f\x96\xa7  Coordinator (server)", cfgBar);
    auto* workerRb= new QRadioButton("\xf0\x9f\x96\xa5  Worker (client)",       cfgBar);
    coordRb->setChecked(true);
    auto* modeBg = new QButtonGroup(cfgBar);
    modeBg->addButton(coordRb, 0);
    modeBg->addButton(workerRb, 1);

    m_localChk = new QCheckBox("Usa questo PC come worker", cfgBar);
    m_localChk->setChecked(true);
    m_localChk->setToolTip("Il coordinator esegue anche task locali");

    m_btnStartStop = new QPushButton(
        "\xf0\x9f\x9f\xa2  Avvia Coordinator", cfgBar);
    m_btnStartStop->setObjectName("primaryBtn");

    cfgLay->addWidget(coordRb);
    cfgLay->addWidget(workerRb);
    cfgLay->addSpacing(dpiScale(12));
    cfgLay->addWidget(m_localChk);
    cfgLay->addStretch(1);
    cfgLay->addWidget(m_btnStartStop);
    rootLay->addWidget(cfgBar);

    /* ── Token + host (stack: coordinator vs worker) ── */
    m_modeStack = new QStackedWidget(root);

    /* Coordinator: solo token */
    auto* coordCfg = new QWidget;
    auto* ccLay    = new QHBoxLayout(coordCfg);
    ccLay->setContentsMargins(0,0,0,0); ccLay->setSpacing(dpiScale(6));
    ccLay->addWidget(new QLabel("Token:", coordCfg));
    m_tokenEdit = new QLineEdit(coordCfg);
    m_tokenEdit->setEchoMode(QLineEdit::Password);
    m_tokenEdit->setText(m_token);
    m_tokenEdit->setToolTip("Token condiviso con tutti i worker (VPN Tailscale consigliata)");
    m_tokenEdit->setFixedWidth(dpiScale(220));
    auto* btnShowToken = new QPushButton("\xf0\x9f\x91\x81", coordCfg);
    btnShowToken->setFixedWidth(dpiScale(30));
    btnShowToken->setObjectName("actionBtn");
    connect(btnShowToken, &QPushButton::clicked, this, [this] {
        m_tokenEdit->setEchoMode(
            m_tokenEdit->echoMode() == QLineEdit::Password
            ? QLineEdit::Normal : QLineEdit::Password);
    });
    ccLay->addWidget(m_tokenEdit);
    ccLay->addWidget(btnShowToken);
    ccLay->addStretch(1);
    m_modeStack->addWidget(coordCfg);   /* index 0 */

    /* Worker: host + token + connect */
    auto* workerCfg = new QWidget;
    auto* wcLay     = new QHBoxLayout(workerCfg);
    wcLay->setContentsMargins(0,0,0,0); wcLay->setSpacing(dpiScale(6));
    wcLay->addWidget(new QLabel("Coordinator IP:", workerCfg));
    m_coordHostEdit = new QLineEdit(workerCfg);
    m_coordHostEdit->setPlaceholderText("100.64.0.1  (IP Tailscale)");
    m_coordHostEdit->setFixedWidth(dpiScale(160));
    wcLay->addWidget(m_coordHostEdit);
    wcLay->addWidget(new QLabel("Token:", workerCfg));
    auto* wTokenEdit = new QLineEdit(workerCfg);
    wTokenEdit->setEchoMode(QLineEdit::Password);
    wTokenEdit->setFixedWidth(dpiScale(180));
    wTokenEdit->setToolTip("Deve corrispondere al token del coordinator");
    connect(wTokenEdit, &QLineEdit::textChanged, this,
            [this](const QString& t){ m_token = t; });
    wcLay->addWidget(wTokenEdit);
    m_btnConnect = new QPushButton("\xf0\x9f\x94\x97  Connetti", workerCfg);
    m_btnConnect->setObjectName("primaryBtn");
    wcLay->addWidget(m_btnConnect);
    wcLay->addStretch(1);
    m_modeStack->addWidget(workerCfg);  /* index 1 */

    rootLay->addWidget(m_modeStack);

    /* Status bar */
    m_statusLbl = new QLabel(
        "<span style='color:#6b7280;'>\xe2\x9a\xaa Inattivo</span>", root);
    m_statusLbl->setTextFormat(Qt::RichText);
    m_statusLbl->setObjectName("cardDesc");
    rootLay->addWidget(m_statusLbl);

    /* ── Corpo principale: splitter verticale ── */
    auto* splitter = new QSplitter(Qt::Vertical, root);

    /* Top: creazione WU + tabella WU */
    auto* topWidget = new QWidget;
    auto* topLay    = new QHBoxLayout(topWidget);
    topLay->setContentsMargins(0,0,0,0);
    topLay->setSpacing(dpiScale(8));

    /* Pannello sinistra: form creazione WU */
    auto* createGroup = new QGroupBox(
        "\xe2\x9e\x95  Nuova Work Unit", topWidget);
    auto* formLay = new QVBoxLayout(createGroup);
    formLay->setSpacing(dpiScale(5));

    /* Tipo task */
    auto* typeRow = new QHBoxLayout;
    typeRow->addWidget(new QLabel("Tipo:", createGroup));
    m_typeCombo = new QComboBox(createGroup);
    m_typeCombo->setObjectName("settingCombo");
    for (const auto& t : taskTypes()) {
        m_typeCombo->addItem(
            QString("[%1]  %2").arg(t.domain, t.name), t.id);
    }
    typeRow->addWidget(m_typeCombo, 1);
    formLay->addLayout(typeRow);

    /* Label */
    auto* lblRow = new QHBoxLayout;
    lblRow->addWidget(new QLabel("Label:", createGroup));
    m_labelEdit = new QLineEdit(createGroup);
    m_labelEdit->setPlaceholderText("Descrizione breve (opzionale)");
    lblRow->addWidget(m_labelEdit, 1);
    formLay->addLayout(lblRow);

    /* Params JSON */
    formLay->addWidget(new QLabel("Parametri JSON:", createGroup));
    m_paramsEdit = new QTextEdit(createGroup);
    m_paramsEdit->setFixedHeight(dpiScale(110));
    m_paramsEdit->setPlaceholderText("{}");
    m_paramsEdit->setObjectName("codeEdit");
    formLay->addWidget(m_paramsEdit);

    /* Priorità + Repliche */
    auto* prioRow = new QHBoxLayout;
    prioRow->addWidget(new QLabel("Priorit\xc3\xa0:", createGroup));
    m_priorityCmb = new QComboBox(createGroup);
    m_priorityCmb->setObjectName("settingCombo");
    m_priorityCmb->addItem("1 — Normale", 1);
    m_priorityCmb->addItem("2 — Alta",    2);
    m_priorityCmb->addItem("3 — Urgente", 3);
    prioRow->addWidget(m_priorityCmb);
    prioRow->addSpacing(dpiScale(8));
    prioRow->addWidget(new QLabel("Repliche:", createGroup));
    m_replicasCmb = new QComboBox(createGroup);
    m_replicasCmb->setObjectName("settingCombo");
    m_replicasCmb->addItem("1 (nessuna validazione)", 1);
    m_replicasCmb->addItem("2 (quorum validation)",   2);
    m_replicasCmb->addItem("3 (quorum 3 nodi)",       3);
    m_replicasCmb->setToolTip(
        "Repliche > 1: stesso WU su N nodi, confronto hash SHA-256");
    prioRow->addWidget(m_replicasCmb);
    prioRow->addStretch(1);
    formLay->addLayout(prioRow);

    auto* addBtn = new QPushButton(
        "\xe2\x9e\x95  Aggiungi alla coda", createGroup);
    addBtn->setObjectName("primaryBtn");
    formLay->addWidget(addBtn);

    createGroup->setFixedWidth(dpiScale(320));
    topLay->addWidget(createGroup);

    /* Pannello destra: tabella WU */
    auto* wuGroup = new QGroupBox(
        "\xf0\x9f\x93\x8b  Coda Work Units", topWidget);
    auto* wuGLay  = new QVBoxLayout(wuGroup);
    wuGLay->setSpacing(dpiScale(4));

    m_wuTable = new QTableWidget(0, 7, wuGroup);
    m_wuTable->setHorizontalHeaderLabels(
        {"ID", "Tipo", "Label", "Status", "Nodo", "Prior.", "Creato"});
    m_wuTable->horizontalHeader()->setSectionResizeMode(2, QHeaderView::Stretch);
    m_wuTable->setColumnWidth(0, dpiScale(70));
    m_wuTable->setColumnWidth(1, dpiScale(90));
    m_wuTable->setColumnWidth(3, dpiScale(80));
    m_wuTable->setColumnWidth(4, dpiScale(80));
    m_wuTable->setColumnWidth(5, dpiScale(45));
    m_wuTable->setColumnWidth(6, dpiScale(55));
    m_wuTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_wuTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_wuTable->verticalHeader()->hide();
    m_wuTable->setAlternatingRowColors(true);
    wuGLay->addWidget(m_wuTable, 1);

    auto* wuBtnRow = new QHBoxLayout;
    auto* btnRefWu = new QPushButton("\xf0\x9f\x94\x84", wuGroup);
    btnRefWu->setObjectName("actionBtn");
    btnRefWu->setToolTip("Aggiorna lista WU");
    auto* btnDelWu = new QPushButton("\xf0\x9f\x97\x91  Elimina", wuGroup);
    btnDelWu->setObjectName("actionBtn");
    btnDelWu->setToolTip("Elimina WU selezionata (solo se non in esecuzione)");
    wuBtnRow->addWidget(btnRefWu);
    wuBtnRow->addWidget(btnDelWu);
    wuBtnRow->addStretch(1);
    wuGLay->addLayout(wuBtnRow);

    topLay->addWidget(wuGroup, 1);
    splitter->addWidget(topWidget);

    /* Bottom: nodi + risultati */
    auto* bottomTabs = new QTabWidget;
    bottomTabs->setDocumentMode(true);

    /* Tab nodi */
    auto* nodeWidget = new QWidget;
    auto* nodeLay    = new QVBoxLayout(nodeWidget);
    nodeLay->setContentsMargins(4,4,4,4);
    m_nodeTable = new QTableWidget(0, 7, nodeWidget);
    m_nodeTable->setHorizontalHeaderLabels(
        {"ID", "Nome", "Indirizzo", "CPU", "RAM (GB)", "GPU", "Tool disponibili"});
    m_nodeTable->horizontalHeader()->setSectionResizeMode(6, QHeaderView::Stretch);
    m_nodeTable->setColumnWidth(0, dpiScale(70));
    m_nodeTable->setColumnWidth(1, dpiScale(90));
    m_nodeTable->setColumnWidth(2, dpiScale(110));
    m_nodeTable->setColumnWidth(3, dpiScale(40));
    m_nodeTable->setColumnWidth(4, dpiScale(60));
    m_nodeTable->setColumnWidth(5, dpiScale(110));
    m_nodeTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_nodeTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_nodeTable->verticalHeader()->hide();
    m_nodeTable->setAlternatingRowColors(true);
    nodeLay->addWidget(m_nodeTable);
    bottomTabs->addTab(nodeWidget,
        "\xf0\x9f\x96\xa5  Nodi (%1)");

    /* Tab risultati */
    auto* resWidget = new QWidget;
    auto* resLay    = new QVBoxLayout(resWidget);
    resLay->setContentsMargins(4,4,4,4);
    m_resultView = new QTextEdit(resWidget);
    m_resultView->setReadOnly(true);
    m_resultView->setObjectName("codeEdit");
    m_resultView->setPlaceholderText("Seleziona una WU nella tabella per vedere il risultato...");
    resLay->addWidget(m_resultView);
    bottomTabs->addTab(resWidget, "\xf0\x9f\x93\x8a  Risultati");

    /* Tab log */
    auto* logWidget = new QWidget;
    auto* logLay    = new QVBoxLayout(logWidget);
    logLay->setContentsMargins(4,4,4,4);
    m_logView = new QTextEdit(logWidget);
    m_logView->setReadOnly(true);
    m_logView->setObjectName("codeEdit");
    logLay->addWidget(m_logView);
    bottomTabs->addTab(logWidget, "\xf0\x9f\x93\x9c  Log");

    /* ── Tab Strutture proteiche ── */
    m_proteinWidget = new SciProteinWidget(bottomTabs);
    bottomTabs->addTab(m_proteinWidget,
        "\xf0\x9f\xa7\xac  Proteine / 3D");
    /* Quando l'utente predice una struttura, offre di creare WU docking */
    connect(m_proteinWidget, &SciProteinWidget::pdbReady,
            this, [this](const QString& pdbData, const QString& label) {
        /* Registra il PDB come file broker e suggerisci AutoDock */
        const QString tmpPath = QDir::tempPath() + "/" + label + ".pdb";
        QFile f(tmpPath);
        if (f.open(QIODevice::WriteOnly)) f.write(pdbData.toUtf8());
        registerFile(label + ".pdb", tmpPath);
        appendLog(QString("\xf0\x9f\xa7\xaa  Struttura '%1' pronta. "
                          "Usa il tab Pipeline per avviare il docking.").arg(label));
    });

    /* ── Tab Pipeline ── */
    auto* pipWidget = new QWidget(bottomTabs);
    auto* pipLay    = new QVBoxLayout(pipWidget);
    pipLay->setContentsMargins(dpiScale(8), dpiScale(8), dpiScale(8), dpiScale(8));
    pipLay->setSpacing(dpiScale(8));

    pipLay->addWidget(new QLabel(
        "<b>\xe2\x9b\x93  Pipeline scientifiche predefinite</b>"
        "  <span style='color:gray;font-size:11px;'>"
        "Sequenza di task con dipendenze automatiche</span>", pipWidget));

    auto* pipGrid = new QGridLayout;
    pipGrid->setSpacing(dpiScale(6));

    auto makePipBtn = [&](const QString& icon, const QString& name,
                           const QString& desc, const QString& tmplId) {
        auto* btn = new QPushButton(icon + "  " + name, pipWidget);
        btn->setObjectName("actionBtn");
        btn->setToolTip(desc);
        btn->setMinimumHeight(dpiScale(42));
        connect(btn, &QPushButton::clicked, this,
                [this, tmplId] { createPipeline(tmplId); });
        return btn;
    };

    pipGrid->addWidget(makePipBtn(
        "\xf0\x9f\xa7\xac", "Protein Fold → Docking",
        "ESMFold API → AutoDock Vina\n"
        "Input: sequenza amminoacidica + file ligando\n"
        "Output: struttura PDB + affinità binding (kcal/mol)",
        "protein_fold_dock"), 0, 0);

    pipGrid->addWidget(makePipBtn(
        "\xf0\x9f\xa7\xac", "NGS Variant Calling",
        "FastQC → BWA align → SAMtools → R variant analysis\n"
        "Input: FASTQ reads + genoma di riferimento\n"
        "Output: report QC + BAM allineato + varianti",
        "ngs_variant"), 0, 1);

    pipGrid->addWidget(makePipBtn(
        "\xf0\x9f\x94\xac", "MD Simulation",
        "GROMACS Minimizzazione → MD Production → Analisi Python\n"
        "Input: file .tpr GROMACS\n"
        "Output: traiettoria + RMSD + analisi energetica",
        "md_sim"), 1, 0);

    pipGrid->addWidget(makePipBtn(
        "\xf0\x9f\xa7\xaa", "Docking da PDB",
        "Scarica struttura RCSB → AutoDock Vina → R binding analysis\n"
        "Apri prima il tab Proteine/3D, poi avvia questa pipeline",
        "pdb_dock"), 1, 1);

    pipLay->addLayout(pipGrid);

    /* Stato pipeline attive */
    auto* pipStatusLbl = new QLabel(
        "<small>Le pipeline rispettano l'ordine: ogni step parte solo quando il precedente "
        "ha status <b>done</b> o <b>validated</b>. Monitorale nel tab Work Units.</small>",
        pipWidget);
    pipStatusLbl->setWordWrap(true);
    pipStatusLbl->setObjectName("hintLabel");
    pipStatusLbl->setTextFormat(Qt::RichText);
    pipLay->addWidget(pipStatusLbl);
    pipLay->addStretch(1);

    bottomTabs->addTab(pipWidget, "\xe2\x9b\x93  Pipeline");

    splitter->addWidget(bottomTabs);
    splitter->setStretchFactor(0, 3);
    splitter->setStretchFactor(1, 2);

    rootLay->addWidget(splitter, 1);

    /* ── Connessioni ── */
    connect(modeBg, &QButtonGroup::idClicked, this, [this](int id) {
        m_isCoord = (id == 0);
        if (m_modeStack) m_modeStack->setCurrentIndex(id);
        if (m_localChk)  m_localChk->setVisible(id == 0);
        if (m_btnStartStop) m_btnStartStop->setVisible(id == 0);
        if (m_btnConnect)   m_btnConnect->setVisible(id == 1);
    });
    connect(m_localChk, &QCheckBox::toggled, this,
            [this](bool v){ m_useLocal = v; });
    connect(m_tokenEdit, &QLineEdit::textChanged, this,
            [this](const QString& t){ m_token = t; });
    connect(m_btnStartStop, &QPushButton::clicked,
            this, &SciComputePage::onStartStopClicked);
    connect(m_btnConnect, &QPushButton::clicked,
            this, &SciComputePage::onConnectClicked);
    connect(addBtn, &QPushButton::clicked,
            this, &SciComputePage::onAddWuClicked);
    connect(m_typeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &SciComputePage::onTypeComboChanged);
    connect(m_wuTable, &QTableWidget::cellClicked,
            this, &SciComputePage::onWuTableRowClicked);
    connect(btnRefWu, &QPushButton::clicked,
            this, [this]{ refreshWuTable(); refreshNodeTable(); });
    connect(btnDelWu, &QPushButton::clicked,
            this, &SciComputePage::onDeleteWuClicked);

    /* Pre-carica template primo tipo */
    onTypeComboChanged(0);

    /* Carica dati iniziali */
    refreshWuTable();
    refreshNodeTable();

    return root;
}

/* ══════════════════════════════════════════════════════════════
   refresh — Tabelle e viste
   ══════════════════════════════════════════════════════════════ */

void SciComputePage::refreshWuTable()
{
    if (!m_wuTable) return;
    const auto rows = queryWus();
    m_wuTable->setRowCount(rows.size());
    for (int i = 0; i < rows.size(); ++i) {
        const auto& r = rows[i];
        const QString id     = r["id"].toString();
        const QString status = r["status"].toString();
        const qint64  ts     = r["created"].toLongLong();
        const QString time   = QDateTime::fromSecsSinceEpoch(ts).toString("HH:mm");

        auto setCell = [&](int col, const QString& text, const QString& color = {}) {
            auto* item = new QTableWidgetItem(text);
            if (!color.isEmpty())
                item->setForeground(QColor(color));
            item->setData(Qt::UserRole, id);
            m_wuTable->setItem(i, col, item);
        };

        setCell(0, id.left(8));
        setCell(1, r["type"].toString());
        setCell(2, r["label"].toString().isEmpty() ? r["type"].toString() : r["label"].toString());
        setCell(3, statusIcon(status) + " " + status, statusColor(status));
        setCell(4, r["node"].toString().left(8));
        setCell(5, r["priority"].toString());
        setCell(6, time);

        m_wuTable->setRowHeight(i, dpiScale(22));
    }
}

void SciComputePage::refreshNodeTable()
{
    if (!m_nodeTable) return;
    const auto rows = queryNodes();
    m_nodeTable->setRowCount(rows.size());
    for (int i = 0; i < rows.size(); ++i) {
        const auto& r      = rows[i];
        const QString id   = r["id"].toString();
        const QString st   = r["status"].toString();
        const QString stColor = (st=="idle") ? "#22c55e"
                             : (st=="busy") ? "#3b82f6"
                             : "#ef4444";

        auto setCell = [&](int col, const QString& text, const QString& color = {}) {
            auto* item = new QTableWidgetItem(text);
            if (!color.isEmpty())
                item->setForeground(QColor(color));
            m_nodeTable->setItem(i, col, item);
        };

        /* Evidenzia nodo locale */
        const QString dispId = (id == m_myNodeId) ? id.left(8) + " (io)" : id.left(8);
        setCell(0, dispId, id == m_myNodeId ? "#a78bfa" : QString());
        setCell(1, r["name"].toString() + "  [" + st + "]", stColor);
        setCell(2, r["addr"].toString() + ":" + r["port"].toString());
        setCell(3, r["cpu"].toString());
        setCell(4, r["ram"].toString());
        setCell(5, r["gpu"].toString().isEmpty() ? "—" : r["gpu"].toString());

        /* Tool: lista compatta */
        const QJsonArray toolsArr =
            QJsonDocument::fromJson(r["tools"].toString().toUtf8()).array();
        QStringList tList;
        for (const auto& v : toolsArr) tList << v.toString();
        setCell(6, tList.join("  "));

        m_nodeTable->setRowHeight(i, dpiScale(22));
    }
}

void SciComputePage::refreshResults(const QString& wuId)
{
    if (!m_resultView) return;
    if (wuId.isEmpty()) {
        m_resultView->setPlainText("Seleziona una WU nella tabella per vedere il risultato.");
        return;
    }
    const auto results = queryResults(wuId);
    if (results.isEmpty()) {
        m_resultView->setPlainText("Nessun risultato ancora per WU: " + wuId.left(8));
        return;
    }
    QString text;
    for (const auto& r : results) {
        const QString nName = r["node_name"].toString().isEmpty()
                            ? r["node_id"].toString().left(8)
                            : r["node_name"].toString();
        const qint64 ts = r["ts"].toLongLong();
        text += QString("═══ Nodo: %1  |  %2  |  Hash: %3 ═══\n")
                .arg(nName)
                .arg(QDateTime::fromSecsSinceEpoch(ts).toString("dd/MM HH:mm:ss"))
                .arg(r["hash"].toString().left(16) + "...");
        text += r["output"].toString();
        text += "\n\n";
    }
    m_resultView->setPlainText(text.trimmed());
}

void SciComputePage::appendLog(const QString& msg)
{
    if (!m_logView) return;
    const QString ts = QDateTime::currentDateTime().toString("HH:mm:ss");
    m_logView->append("[" + ts + "]  " + msg);
}
