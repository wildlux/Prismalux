#include "widget_webdev.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QGroupBox>
#include <QSplitter>
#include <QTabWidget>
#include <QFileDialog>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QDirIterator>
#include <QProcess>
#include <QTimer>
#include <QTextStream>
#include <QScrollBar>
#include <QDesktopServices>
#include <QUrl>
#include <QLabel>
#include <QLineEdit>
#include <QComboBox>
#include <QPushButton>
#include <QPlainTextEdit>
#include <QTextEdit>
#include <QListWidget>
#include <QListWidgetItem>
#include <QFont>
#include <QFontDatabase>
#include <QCheckBox>
#include <QProcessEnvironment>
#include <QMessageBox>
#include "../dpi_utils.h"

/* ─── costanti ─────────────────────────────────────────── */
static constexpr int kLargeProjectThreshold = 40;   // file .py oltre cui mostrare la selezione
static constexpr int kMaxFileReadKb         = 64;   // max KB letti per file nell'analisi

/* ─── Costruttore ──────────────────────────────────────── */
WebDevWidget::WebDevWidget(AiClient* ai, QWidget* parent)
    : QWidget(parent), m_ai(ai)
{
    buildLayout();
    setupConnections();
    QTimer::singleShot(0, this, &WebDevWidget::onPopulateModels);
}

WebDevWidget::~WebDevWidget()
{
    if (m_serverProc && m_serverProc->state() != QProcess::NotRunning) {
        m_serverProc->kill();
        m_serverProc->waitForFinished(1000);
    }
    disconnectAi();
}

/* ─── Build layout ─────────────────────────────────────── */
void WebDevWidget::buildLayout()
{
    auto* lay = new QVBoxLayout(this);
    lay->setContentsMargins(dpiScale(8), dpiScale(6), dpiScale(8), dpiScale(6));
    lay->setSpacing(dpiScale(6));

    lay->addWidget(buildProjectRow(this));
    lay->addWidget(buildControlSplitter(this));
    lay->addWidget(buildOutputArea(this), 1);
}

QWidget* WebDevWidget::buildProjectRow(QWidget* parent)
{
    auto* box  = new QGroupBox(parent);
    auto* lay  = new QVBoxLayout(box);
    lay->setContentsMargins(dpiScale(8), dpiScale(6), dpiScale(8), dpiScale(6));
    lay->setSpacing(dpiScale(4));

    auto* row = new QWidget(box);
    auto* hl  = new QHBoxLayout(row);
    hl->setContentsMargins(0, 0, 0, 0);
    hl->setSpacing(dpiScale(6));

    hl->addWidget(new QLabel("Cartella progetto:", row));
    m_projectPath = new QLineEdit(row);
    m_projectPath->setPlaceholderText("/home/user/mio_progetto_django  …");
    hl->addWidget(m_projectPath, 1);

    auto* btnBrowse = new QPushButton("\xf0\x9f\x93\x82  Sfoglia", row);
    btnBrowse->setFixedWidth(dpiScale(100));
    hl->addWidget(btnBrowse);

    lay->addWidget(row);

    m_detectLbl = new QLabel("Nessun progetto selezionato.", box);
    m_detectLbl->setStyleSheet("color:#94a3b8; font-style:italic;");
    lay->addWidget(m_detectLbl);

    connect(btnBrowse, &QPushButton::clicked, this, &WebDevWidget::onBrowseClicked);
    connect(m_projectPath, &QLineEdit::editingFinished, this, [this] {
        detectFramework(m_projectPath->text().trimmed());
    });

    return box;
}

QWidget* WebDevWidget::buildControlSplitter(QWidget* parent)
{
    auto* split = new QSplitter(Qt::Horizontal, parent);
    split->setChildrenCollapsible(false);
    split->addWidget(buildServerGroup(split));
    split->addWidget(buildAiGroup(split));
    split->setStretchFactor(0, 1);
    split->setStretchFactor(1, 1);
    return split;
}

QGroupBox* WebDevWidget::buildServerGroup(QWidget* parent)
{
    auto* grp = new QGroupBox("\xf0\x9f\x8c\x90  Server di sviluppo", parent);
    auto* lay = new QVBoxLayout(grp);
    lay->setSpacing(dpiScale(6));

    auto* form = new QFormLayout();
    form->setSpacing(dpiScale(5));

    m_fwCombo = new QComboBox(grp);
    m_fwCombo->addItem("Django",  "django");
    m_fwCombo->addItem("Flask",   "flask");
    m_fwCombo->addItem("FastAPI (uvicorn)", "fastapi");
    m_fwCombo->addItem("Uvicorn generico",  "uvicorn");
    m_fwCombo->addItem("Gunicorn", "gunicorn");
    m_fwCombo->addItem("Bottle",   "bottle");
    form->addRow("Framework:", m_fwCombo);

    m_portEdit = new QLineEdit("8000", grp);
    m_portEdit->setMaximumWidth(dpiScale(80));
    form->addRow("Porta:", m_portEdit);

    m_argsEdit = new QLineEdit(grp);
    m_argsEdit->setPlaceholderText("es. --settings=config.dev");
    form->addRow("Args extra:", m_argsEdit);

    lay->addLayout(form);

    /* pulsanti Avvia/Ferma */
    auto* btnRow = new QWidget(grp);
    auto* btnLay = new QHBoxLayout(btnRow);
    btnLay->setContentsMargins(0, 0, 0, 0);
    btnLay->setSpacing(dpiScale(6));
    m_btnStart = new QPushButton("\xe2\x96\xb6  Avvia", btnRow);
    m_btnStart->setStyleSheet("QPushButton{background:#16a34a;color:#fff;font-weight:bold;border-radius:4px;padding:4px 10px;}"
                              "QPushButton:hover{background:#15803d;}");
    m_btnStop  = new QPushButton("\xe2\x96\xa0  Ferma", btnRow);
    m_btnStop->setEnabled(false);
    m_btnStop->setStyleSheet("QPushButton{background:#dc2626;color:#fff;font-weight:bold;border-radius:4px;padding:4px 10px;}"
                             "QPushButton:disabled{background:#6b7280;}");
    m_btnOpen = new QPushButton("\xf0\x9f\x8c\x90  Apri", btnRow);
    m_btnOpen->setEnabled(false);
    m_btnOpen->setToolTip("Apri nel browser");
    m_btnOpen->setStyleSheet("QPushButton{background:#0891b2;color:#fff;font-weight:bold;border-radius:4px;padding:4px 10px;}"
                             "QPushButton:hover{background:#0e7490;}"
                             "QPushButton:disabled{background:#6b7280;}");
    m_chkDebug = new QCheckBox("DEBUG", btnRow);
    m_chkDebug->setChecked(true);
    m_chkDebug->setToolTip("Imposta DEBUG=True — abilita la gestione dei file statici e il reloader");

    m_debugBadge = new QLabel(btnRow);
    m_debugBadge->setFixedHeight(dpiScale(22));
    m_debugBadge->setAlignment(Qt::AlignCenter);
    m_debugBadge->setContentsMargins(dpiScale(6), 0, dpiScale(6), 0);

    btnLay->addWidget(m_btnStart);
    btnLay->addWidget(m_btnStop);
    btnLay->addWidget(m_btnOpen);
    btnLay->addSpacing(dpiScale(8));
    btnLay->addWidget(m_chkDebug);
    btnLay->addWidget(m_debugBadge);
    btnLay->addStretch();

    onDebugToggled(true);  // inizializza il badge
    lay->addWidget(btnRow);

    m_serverStatus = new QLabel("\xe2\x9a\xab Fermo", grp);
    m_serverStatus->setStyleSheet("color:#94a3b8; font-size:11px;");
    lay->addWidget(m_serverStatus);

    lay->addStretch();
    return grp;
}

QGroupBox* WebDevWidget::buildAiGroup(QWidget* parent)
{
    auto* grp = new QGroupBox("\xf0\x9f\xa4\x96  Analisi AI del progetto", parent);
    auto* lay = new QVBoxLayout(grp);
    lay->setSpacing(dpiScale(6));

    auto* form = new QFormLayout();
    form->setSpacing(dpiScale(5));

    m_modelCombo = new QComboBox(grp);
    m_modelCombo->setPlaceholderText("Carico modelli…");
    form->addRow("Modello:", m_modelCombo);

    m_focusCombo = new QComboBox(grp);
    m_focusCombo->addItem("Architettura generale");
    m_focusCombo->addItem("Sicurezza (OWASP Top 10)");
    m_focusCombo->addItem("Performance & caching");
    m_focusCombo->addItem("Qualit\xc3\xa0 codice & bug");
    m_focusCombo->addItem("API & endpoint");
    m_focusCombo->addItem("Modelli DB & migrazioni");
    m_focusCombo->addItem("Test & copertura");
    form->addRow("Focus:", m_focusCombo);

    lay->addLayout(form);

    auto* btnRow = new QWidget(grp);
    auto* btnLay = new QHBoxLayout(btnRow);
    btnLay->setContentsMargins(0, 0, 0, 0);
    btnLay->setSpacing(dpiScale(6));

    m_btnAnalyze  = new QPushButton("\xf0\x9f\x94\x8d  Analizza", btnRow);
    m_btnAnalyze->setStyleSheet("QPushButton{background:#2563eb;color:#fff;font-weight:bold;border-radius:4px;padding:4px 10px;}"
                                "QPushButton:hover{background:#1d4ed8;}"
                                "QPushButton:disabled{background:#6b7280;}");
    m_btnWhatToDo = new QPushButton("\xe2\x9d\x93  Cosa fare?", btnRow);
    m_btnWhatToDo->setToolTip("L'AI esamina il progetto e propone una lista di possibili azioni/miglioramenti");
    m_btnWhatToDo->setStyleSheet("QPushButton{background:#7c3aed;color:#fff;font-weight:bold;border-radius:4px;padding:4px 10px;}"
                                 "QPushButton:hover{background:#6d28d9;}"
                                 "QPushButton:disabled{background:#6b7280;}");
    btnLay->addWidget(m_btnAnalyze);
    btnLay->addWidget(m_btnWhatToDo);
    btnLay->addStretch();
    lay->addWidget(btnRow);

    /* pannello selezione moduli (visibile solo per progetto grande) */
    m_moduleBox  = new QGroupBox("Seleziona cosa analizzare:", grp);
    auto* mbLay  = new QVBoxLayout(m_moduleBox);
    m_moduleList = new QListWidget(m_moduleBox);
    m_moduleList->setSelectionMode(QAbstractItemView::MultiSelection);
    m_moduleList->setMaximumHeight(dpiScale(120));
    mbLay->addWidget(m_moduleList);
    m_btnAnalyzeSel = new QPushButton("\xf0\x9f\x94\x8d  Analizza selezione", m_moduleBox);
    m_btnAnalyzeSel->setStyleSheet("QPushButton{background:#0891b2;color:#fff;border-radius:4px;padding:4px 8px;}"
                                   "QPushButton:hover{background:#0e7490;}");
    mbLay->addWidget(m_btnAnalyzeSel);
    m_moduleBox->setVisible(false);
    lay->addWidget(m_moduleBox);

    lay->addStretch();
    return grp;
}

QWidget* WebDevWidget::buildOutputArea(QWidget* parent)
{
    m_outTabs = new QTabWidget(parent);

    /* Tab Server */
    m_serverLog = new QPlainTextEdit(m_outTabs);
    m_serverLog->setReadOnly(true);
    m_serverLog->setMaximumBlockCount(2000);
    const QFont mono = QFontDatabase::systemFont(QFontDatabase::FixedFont);
    m_serverLog->setFont(mono);
    m_outTabs->addTab(m_serverLog, "\xf0\x9f\x96\xa5  Server");

    /* Tab AI */
    m_aiOutput = new QTextEdit(m_outTabs);
    m_aiOutput->setReadOnly(true);
    m_outTabs->addTab(m_aiOutput, "\xf0\x9f\xa4\x96  AI");

    return m_outTabs;
}

/* ─── Connessioni ──────────────────────────────────────── */
void WebDevWidget::setupConnections()
{
    connect(m_btnStart,       &QPushButton::clicked, this, &WebDevWidget::onStartClicked);
    connect(m_btnStop,        &QPushButton::clicked, this, &WebDevWidget::onStopClicked);
    connect(m_btnOpen,        &QPushButton::clicked, this, &WebDevWidget::onOpenBrowser);
    connect(m_btnAnalyze,     &QPushButton::clicked, this, &WebDevWidget::onAnalyzeClicked);
    connect(m_btnWhatToDo,    &QPushButton::clicked, this, &WebDevWidget::onWhatToDoClicked);
    connect(m_btnAnalyzeSel,  &QPushButton::clicked, this, &WebDevWidget::onAnalyzeSelClicked);
    connect(m_fwCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &WebDevWidget::onFwComboChanged);
    connect(m_chkDebug, &QCheckBox::toggled,
            this, &WebDevWidget::onDebugToggled);
}

/* ─── Slot UI ──────────────────────────────────────────── */
void WebDevWidget::onBrowseClicked()
{
    const QString dir = QFileDialog::getExistingDirectory(
        this, "Seleziona cartella progetto",
        m_projectPath->text().isEmpty() ? QDir::homePath() : m_projectPath->text());
    if (dir.isEmpty()) return;
    m_projectPath->setText(dir);
    detectFramework(dir);
}

void WebDevWidget::onFwComboChanged(int /*idx*/)
{
    const QString fw = m_fwCombo->currentData().toString();
    if (fw == "django")
        m_portEdit->setText("8000");
    else if (fw == "flask" || fw == "bottle")
        m_portEdit->setText("5000");
    else
        m_portEdit->setText("8000");
}

void WebDevWidget::onDebugToggled(bool checked)
{
    if (!m_debugBadge) return;
    if (checked) {
        m_debugBadge->setText("DEBUG=True");
        m_debugBadge->setStyleSheet(
            "QLabel{background:#166534;color:#bbf7d0;border:1px solid #16a34a;"
            "border-radius:4px;font-size:11px;font-weight:bold;}");
        m_debugBadge->setToolTip("Il server parte con DEBUG=True\n"
                                 "File statici serviti automaticamente, reloader attivo");
    } else {
        m_debugBadge->setText("DEBUG=False");
        m_debugBadge->setStyleSheet(
            "QLabel{background:#7c2d12;color:#fed7aa;border:1px solid #c2410c;"
            "border-radius:4px;font-size:11px;font-weight:bold;}");
        m_debugBadge->setToolTip("Il server parte con DEBUG=False\n"
                                 "File statici NON serviti — esegui collectstatic");
    }
}

void WebDevWidget::onPopulateModels()
{
    populateModels();
}

/* ─── Rilevamento framework ────────────────────────────── */
void WebDevWidget::detectFramework(const QString& path)
{
    if (path.isEmpty() || !QDir(path).exists()) {
        m_detectLbl->setText("\xe2\x9d\x8c Cartella non trovata.");
        m_detectLbl->setStyleSheet("color:#ef4444; font-style:italic;");
        return;
    }

    QDir d(path);
    bool hasDjango  = d.exists("manage.py");
    bool hasFlask   = d.exists("app.py") || d.exists("application.py");
    bool hasFastapi = d.exists("main.py");
    bool hasWsgi    = d.exists("wsgi.py");

    /* Controlla requirements.txt / pyproject.toml */
    auto readFile = [&](const QString& name) -> QString {
        QFile f(d.filePath(name));
        if (!f.open(QIODevice::ReadOnly)) return {};
        return QString::fromUtf8(f.readAll()).toLower();
    };
    const QString req  = readFile("requirements.txt") + readFile("requirements/base.txt");
    const QString pyp  = readFile("pyproject.toml");
    const QString combo = req + pyp;

    if (hasDjango || combo.contains("django")) {
        m_detectedFw = "django";
        m_fwCombo->setCurrentIndex(0);
        m_portEdit->setText("8000");
        m_detectLbl->setText("\xf0\x9f\x90\x8d  Django rilevato \xe2\x80\x94 manage.py trovato");
    } else if (combo.contains("fastapi") || (hasFastapi && combo.contains("uvicorn"))) {
        m_detectedFw = "fastapi";
        m_fwCombo->setCurrentIndex(2);
        m_portEdit->setText("8000");
        m_detectLbl->setText("\xe2\x9a\xa1  FastAPI rilevato");
    } else if (hasFlask || combo.contains("flask")) {
        m_detectedFw = "flask";
        m_fwCombo->setCurrentIndex(1);
        m_portEdit->setText("5000");
        m_detectLbl->setText("\xf0\x9f\xa7\xaa  Flask rilevato");
    } else if (combo.contains("bottle")) {
        m_detectedFw = "bottle";
        m_fwCombo->setCurrentIndex(5);
        m_portEdit->setText("5000");
        m_detectLbl->setText("\xf0\x9f\x8d\xbe  Bottle rilevato");
    } else if (hasWsgi || combo.contains("gunicorn")) {
        m_detectedFw = "gunicorn";
        m_fwCombo->setCurrentIndex(4);
        m_detectLbl->setText("\xf0\x9f\x9f\xa2  Gunicorn/WSGI rilevato");
    } else {
        m_detectedFw = "generic";
        m_detectLbl->setText("\xe2\x9d\x93  Framework non riconosciuto — seleziona manualmente");
    }
    m_detectLbl->setStyleSheet("color:#22c55e; font-style:normal;");
}

/* ─── Comando di avvio server ──────────────────────────── */
QStringList WebDevWidget::buildRunCommand() const
{
    const QString fw   = m_fwCombo->currentData().toString();
    const QString port = m_portEdit->text().trimmed().isEmpty() ? "8000" : m_portEdit->text().trimmed();
    const QString args = m_argsEdit->text().trimmed();
    QStringList cmd;

    if (fw == "django") {
        cmd << "python3" << "manage.py" << "runserver" << ("0.0.0.0:" + port);
    } else if (fw == "flask") {
        cmd << "flask" << "run" << "--host=0.0.0.0" << ("--port=" + port);
    } else if (fw == "fastapi") {
        /* cerca l'entry point: main.py → main:app; app.py → app:app */
        QDir d(m_projectPath->text());
        const QString entry = d.exists("main.py") ? "main:app" : "app:app";
        cmd << "uvicorn" << entry << "--reload" << "--host" << "0.0.0.0" << "--port" << port;
    } else if (fw == "uvicorn") {
        QDir du(m_projectPath->text());
        const QString uEntry = du.exists("main.py") ? "main:app" : "app:app";
        cmd << "uvicorn" << uEntry << "--reload" << "--host" << "0.0.0.0" << "--port" << port;
    } else if (fw == "gunicorn") {
        QDir dg(m_projectPath->text());
        QString gEntry;
        if (dg.exists("wsgi.py"))       gEntry = "wsgi:application";
        else if (dg.exists("main.py"))  gEntry = "main:app";
        else                            gEntry = "app:app";
        cmd << "gunicorn" << "-w" << "4" << ("-b" + QString("0.0.0.0:") + port) << gEntry;
    } else if (fw == "bottle") {
        cmd << "python3" << "app.py";
    } else {
        cmd << "python3" << "manage.py" << "runserver" << ("0.0.0.0:" + port);
    }

    if (!args.isEmpty())
        cmd << args.split(' ', Qt::SkipEmptyParts);

    return cmd;
}

/* ─── Slot avvio/stop server ───────────────────────────── */
void WebDevWidget::onStartClicked()
{
    const QString path = m_projectPath->text().trimmed();
    if (path.isEmpty() || !QDir(path).exists()) {
        m_serverLog->appendPlainText("[Errore] Seleziona una cartella di progetto valida.");
        return;
    }

    if (!m_serverProc)
        m_serverProc = new QProcess(this);

    if (m_serverProc->state() != QProcess::NotRunning) return;

    const QStringList cmd = buildRunCommand();
    if (cmd.isEmpty()) return;

    connect(m_serverProc, &QProcess::readyReadStandardOutput,
            this, &WebDevWidget::onServerReadyRead);
    connect(m_serverProc, &QProcess::readyReadStandardError,
            this, &WebDevWidget::onServerReadyReadStderr);
    connect(m_serverProc, QOverload<int,QProcess::ExitStatus>::of(&QProcess::finished),
            this, &WebDevWidget::onServerFinished);
    connect(m_serverProc, &QProcess::errorOccurred,
            this, &WebDevWidget::onServerErrorOccurred);

    m_serverProc->setWorkingDirectory(path);

    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    if (m_chkDebug->isChecked())
        env.insert("DEBUG", "True");
    m_serverProc->setProcessEnvironment(env);

    m_serverProc->start(cmd.first(), cmd.mid(1));

    appendServerLog(QString("$ ") + cmd.join(' '));
    setServerRunning(true);
    m_outTabs->setCurrentIndex(0);
}

void WebDevWidget::onStopClicked()
{
    if (!m_serverProc || m_serverProc->state() == QProcess::NotRunning) return;
    m_serverProc->terminate();
    QTimer::singleShot(3000, this, &WebDevWidget::onServerStopTimer);
}

void WebDevWidget::onServerStopTimer()
{
    if (m_serverProc && m_serverProc->state() != QProcess::NotRunning)
        m_serverProc->kill();
}

void WebDevWidget::onServerReadyRead()
{
    if (!m_serverProc) return;
    const QString text = QString::fromUtf8(m_serverProc->readAllStandardOutput());
    appendServerLog(text.trimmed());
}

void WebDevWidget::onServerReadyReadStderr()
{
    if (!m_serverProc) return;
    const QString text = QString::fromUtf8(m_serverProc->readAllStandardError());
    appendServerLog(text.trimmed());
}

void WebDevWidget::onServerFinished(int code, QProcess::ExitStatus)
{
    appendServerLog(QString("[Server terminato — exit code %1]").arg(code));
    setServerRunning(false);
}

void WebDevWidget::onServerErrorOccurred(QProcess::ProcessError err)
{
    const QStringList errNames = { "FailedToStart","Crashed","Timedout","WriteError","ReadError","UnknownError" };
    const int idx = static_cast<int>(err);
    appendServerLog("[Errore processo] " + (idx < errNames.size() ? errNames[idx] : "sconosciuto"));
    if (err == QProcess::FailedToStart)
        appendServerLog("Suggerimento: verifica che il framework sia installato nell'ambiente virtuale (.venv).");
    setServerRunning(false);
}

void WebDevWidget::appendServerLog(const QString& text)
{
    if (text.isEmpty()) return;
    m_serverLog->appendPlainText(text);
    m_serverLog->verticalScrollBar()->setValue(m_serverLog->verticalScrollBar()->maximum());
}

void WebDevWidget::onOpenBrowser()
{
    const QString port = m_portEdit->text().trimmed().isEmpty() ? "8000" : m_portEdit->text().trimmed();
    QDesktopServices::openUrl(QUrl("http://localhost:" + port + "/"));
}

void WebDevWidget::setServerRunning(bool running)
{
    m_btnStart->setEnabled(!running);
    m_btnStop->setEnabled(running);
    m_btnOpen->setEnabled(running);
    const QString port = m_portEdit->text().trimmed();
    if (running)
        m_serverStatus->setText("\xf0\x9f\x9f\xa2 Avviato su :" + port
                                + "  \xe2\x80\x94  http://localhost:" + port);
    else
        m_serverStatus->setText("\xe2\x9a\xab Fermo");
    m_serverStatus->setStyleSheet(running ? "color:#22c55e; font-size:11px;"
                                           : "color:#94a3b8; font-size:11px;");
}

/* ─── Scansione progetto ───────────────────────────────── */
void WebDevWidget::scanProject(const QString& path)
{
    m_projectFiles.clear();
    m_moduleList->clear();

    QDirIterator it(path, {"*.py"}, QDir::Files, QDirIterator::Subdirectories);
    QStringList dirs;
    while (it.hasNext()) {
        const QString fp = it.next();
        /* escludi venv, migrations, __pycache__, .git, node_modules */
        if (fp.contains("/.venv/")   || fp.contains("/venv/")
         || fp.contains("/migrations/") || fp.contains("/__pycache__/")
         || fp.contains("/.git/")    || fp.contains("/node_modules/"))
            continue;
        m_projectFiles << fp;
        const QString rel = QDir(path).relativeFilePath(fp);
        const QString topDir = rel.section('/', 0, 0);
        if (!dirs.contains(topDir)) dirs << topDir;
    }

    m_largeProject = (m_projectFiles.size() >= kLargeProjectThreshold);

    /* popola lista moduli (sottocartelle di primo livello) */
    for (const QString& d : dirs) {
        auto* item = new QListWidgetItem(d, m_moduleList);
        item->setFlags(item->flags() | Qt::ItemIsUserCheckable);
        item->setCheckState(Qt::Checked);
    }
}

/* ─── Analisi AI ───────────────────────────────────────── */
void WebDevWidget::onAnalyzeClicked()
{
    const QString path = m_projectPath->text().trimmed();
    if (path.isEmpty() || !QDir(path).exists()) {
        m_aiOutput->setPlainText("Seleziona prima una cartella di progetto.");
        m_outTabs->setCurrentIndex(1);
        return;
    }

    scanProject(path);

    if (m_largeProject) {
        /* mostra pannello selezione e chiedi all'utente cosa analizzare */
        m_moduleBox->setVisible(true);
        m_aiOutput->setPlainText(
            QString("\xf0\x9f\x93\x8a  Progetto grande rilevato: %1 file Python.\n\n"
                    "Seleziona i moduli da analizzare nella lista qui sopra,\n"
                    "poi clicca \"\xf0\x9f\x94\x8d Analizza selezione\".\n\n"
                    "Oppure usa \"\xe2\x9d\x93 Cosa fare?\" per avere un'analisi\n"
                    "ad alto livello della struttura del progetto.")
            .arg(m_projectFiles.size()));
        m_outTabs->setCurrentIndex(1);
    } else {
        /* progetto piccolo: analisi diretta */
        m_moduleBox->setVisible(false);
        runAnalysis(m_projectFiles);
    }
}

void WebDevWidget::onAnalyzeSelClicked()
{
    if (m_projectFiles.isEmpty()) return;
    const QString path = m_projectPath->text().trimmed();

    /* costruisce la lista dei file appartenenti ai moduli selezionati */
    QStringList selected;
    for (int i = 0; i < m_moduleList->count(); ++i) {
        auto* item = m_moduleList->item(i);
        if (item && item->checkState() == Qt::Checked) {
            const QString mod = item->text();
            for (const QString& fp : std::as_const(m_projectFiles)) {
                const QString rel = QDir(path).relativeFilePath(fp);
                if (rel.startsWith(mod + '/') || rel == mod)
                    selected << fp;
            }
        }
    }

    if (selected.isEmpty()) {
        m_aiOutput->setPlainText("Seleziona almeno un modulo dalla lista.");
        return;
    }
    runAnalysis(selected);
}

void WebDevWidget::runAnalysis(const QStringList& files)
{
    const QString path   = m_projectPath->text().trimmed();
    const QString focus  = m_focusCombo->currentText();
    const QString fw     = m_fwCombo->currentText();

    /* costruisce il contesto: albero directory + contenuto file (limitato) */
    QString context;
    context += QString("## Progetto: %1\n## Framework: %2\n## Analisi richiesta: %3\n\n")
                   .arg(QDir(path).dirName(), fw, focus);
    context += "### File inclusi nell'analisi:\n";

    qint64 totalBytes = 0;
    constexpr qint64 kMaxTotalKb = 512 * 1024;  // 512 KB totali
    for (const QString& fp : files) {
        if (totalBytes > kMaxTotalKb) {
            context += "\n[... altri file troncati per limite dimensione ...]\n";
            break;
        }
        QFile f(fp);
        if (!f.open(QIODevice::ReadOnly)) continue;
        const QString rel  = QDir(path).relativeFilePath(fp);
        const QByteArray ba = f.read(kMaxFileReadKb * 1024);
        totalBytes += ba.size();
        context += QString("\n#### %1\n```python\n%2\n```\n").arg(rel, QString::fromUtf8(ba));
    }

    const QString sysPrompt =
        "Sei un esperto sviluppatore Python. Analizza il progetto web fornito.\n"
        "Rispondi in italiano. Usa intestazioni Markdown chiare.\n"
        "Per il focus richiesto, fornisci osservazioni specifiche, problemi trovati "
        "e raccomandazioni concrete con esempi di codice dove utile.";

    m_aiOutput->clear();
    m_outTabs->setCurrentIndex(1);
    setAiBusy(true);
    disconnectAi();

    m_aiTokenConn = connect(m_ai, &AiClient::token, this, &WebDevWidget::onAiToken);
    m_aiFinishedConn = connect(m_ai, &AiClient::finished, this, &WebDevWidget::onAiFinished);
    m_aiErrorConn = connect(m_ai, &AiClient::error, this, &WebDevWidget::onAiError);

    m_ai->chat(sysPrompt, context);
}

void WebDevWidget::onWhatToDoClicked()
{
    const QString path = m_projectPath->text().trimmed();
    if (path.isEmpty() || !QDir(path).exists()) {
        m_aiOutput->setPlainText("Seleziona prima una cartella di progetto.");
        m_outTabs->setCurrentIndex(1);
        return;
    }

    scanProject(path);
    runWhatToDo();
}

void WebDevWidget::runWhatToDo()
{
    const QString path = m_projectPath->text().trimmed();
    const QString fw   = m_fwCombo->currentText();

    /* costruisce un albero leggero del progetto (solo nomi file/cartelle, no contenuto) */
    QString tree;
    int count = 0;
    for (const QString& fp : std::as_const(m_projectFiles)) {
        tree += "  " + QDir(path).relativeFilePath(fp) + "\n";
        if (++count >= 120) { tree += "  [... e altri file ...]\n"; break; }
    }

    /* legge solo settings.py / config.py / requirements.txt per contesto */
    QString extraCtx;
    const QStringList keyFiles = {"requirements.txt", "requirements/base.txt",
                                  "settings.py", "config.py", "pyproject.toml",
                                  "Makefile", "docker-compose.yml"};
    for (const QString& kf : keyFiles) {
        for (const QString& fp : std::as_const(m_projectFiles)) {
            if (fp.endsWith("/" + kf) || fp.endsWith(kf)) {
                QFile f(fp);
                if (!f.open(QIODevice::ReadOnly)) continue;
                const QString rel = QDir(path).relativeFilePath(fp);
                extraCtx += QString("\n#### %1\n```\n%2\n```\n")
                    .arg(rel, QString::fromUtf8(f.read(20 * 1024)));
                break;
            }
        }
    }

    const QString userMsg =
        QString("## Progetto: %1 (%2 file Python)\n## Framework: %3\n\n"
                "### Struttura:\n%4\n%5")
        .arg(QDir(path).dirName()).arg(m_projectFiles.size()).arg(fw, tree, extraCtx);

    const QString sysPrompt =
        "Sei un esperto sviluppatore Python senior. Analizza la struttura del progetto web.\n"
        "Rispondi in italiano con una lista numerata di 8-12 azioni/miglioramenti concreti "
        "che si possono fare su questo progetto, ordinati per priorità e impatto.\n"
        "Per ogni punto specifica: cosa fare, perché, e quanto effort stimato.\n"
        "Poi chiedi all'utente quale punto vuole approfondire.";

    m_aiOutput->clear();
    m_outTabs->setCurrentIndex(1);
    setAiBusy(true);
    disconnectAi();

    m_aiTokenConn    = connect(m_ai, &AiClient::token,    this, &WebDevWidget::onAiToken);
    m_aiFinishedConn = connect(m_ai, &AiClient::finished, this, &WebDevWidget::onAiFinished);
    m_aiErrorConn    = connect(m_ai, &AiClient::error,    this, &WebDevWidget::onAiError);

    m_ai->chat(sysPrompt, userMsg);
}

/* ─── Slot AI ──────────────────────────────────────────── */
void WebDevWidget::onAiToken(const QString& tok)
{
    m_aiOutput->moveCursor(QTextCursor::End);
    m_aiOutput->insertPlainText(tok);
    m_aiOutput->verticalScrollBar()->setValue(m_aiOutput->verticalScrollBar()->maximum());
}

void WebDevWidget::onAiFinished(const QString& /*full*/)
{
    setAiBusy(false);
    disconnectAi();
}

void WebDevWidget::onAiError(const QString& msg)
{
    m_aiOutput->append("\n\n\xe2\x9d\x8c Errore AI: " + msg);
    setAiBusy(false);
    disconnectAi();
}

/* ─── Helpers ──────────────────────────────────────────── */
void WebDevWidget::populateModels()
{
    if (!m_ai) return;
    auto* holder = new QObject(this);
    connect(m_ai, &AiClient::modelsReady, holder, [this, holder](const QStringList& list) {
        holder->deleteLater();
        const QString cur = m_modelCombo->currentText();
        m_modelCombo->clear();
        for (const QString& m : list) m_modelCombo->addItem(m, m);
        if (!cur.isEmpty()) {
            const int i = m_modelCombo->findText(cur);
            if (i >= 0) m_modelCombo->setCurrentIndex(i);
        }
    });
    connect(m_ai, &AiClient::error, holder, [holder](const QString&) {
        holder->deleteLater();
    });
    m_ai->fetchModels();
}

void WebDevWidget::setAiBusy(bool busy)
{
    m_btnAnalyze->setEnabled(!busy);
    m_btnWhatToDo->setEnabled(!busy);
    m_btnAnalyzeSel->setEnabled(!busy);
}

void WebDevWidget::disconnectAi()
{
    if (m_aiTokenConn)    { disconnect(m_aiTokenConn);    m_aiTokenConn    = {}; }
    if (m_aiFinishedConn) { disconnect(m_aiFinishedConn); m_aiFinishedConn = {}; }
    if (m_aiErrorConn)    { disconnect(m_aiErrorConn);    m_aiErrorConn    = {}; }
}
