/* ══════════════════════════════════════════════════════════════
   main_app_controller_cloudcompare.cpp — AppControllerPage: CloudCompare
   ===========================================================================
   Tab CLOUDCOMPARE (Python/Open3D scripting) — builder + slot. Split da
   main_app_controller.cpp/main_app_controller_slots.cpp (TODO D-8).
   ══════════════════════════════════════════════════════════════ */
#include "main_app_controller.h"
#include "../prismalux_paths.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGroupBox>
#include <QLabel>
#include <QLineEdit>
#include <QDialog>
#include <QTextBrowser>
#include <QTextEdit>
#include <QPushButton>
#include <QProcess>
#include <QDir>
#include <QFileDialog>
#include <QStandardPaths>

namespace P = PrismaluxPaths;

/* ══════════════════════════════════════════════════════════════
   Tab CLOUDCOMPARE
   ══════════════════════════════════════════════════════════════ */
QWidget* AppControllerPage::buildCloudCompareTab()
{
    auto* w   = new QWidget;
    auto* lay = new QVBoxLayout(w);
    lay->setContentsMargins(16, 16, 16, 16);
    lay->setSpacing(10);

    /* Descrizione */
    auto* descLbl = new QLabel(
        "\xf0\x9f\x94\xb5  <i>CloudCompare \xe2\x80\x94 Software open-source per l\xe2\x80\x99" "elaborazione di "
        "nuvole di punti 3D e mesh poligonali.</i>", w);
    descLbl->setObjectName("hintLabel");
    descLbl->setTextFormat(Qt::RichText);
    descLbl->setWordWrap(true);
    lay->addWidget(descLbl);

    /* Rilevamento installazione + percorso manuale */
    auto* pathGroup = new QGroupBox(tr("\xe2\x9a\x99  Percorso CloudCompare"), w);
    auto* pathLay   = new QHBoxLayout(pathGroup);
    m_ccPathEdit = new QLineEdit(pathGroup);
    m_ccPathEdit->setPlaceholderText(tr("cloudcompare  (auto-rilevato se in PATH)"));
    const QString detected = QStandardPaths::findExecutable("cloudcompare");
    if (!detected.isEmpty()) m_ccPathEdit->setText(detected);
    m_ccStatusLbl = new QLabel(pathGroup);
    m_ccStatusLbl->setWordWrap(false);
    if (detected.isEmpty()) {
        m_ccStatusLbl->setText(
            "<span style='color:#f87171;'>"
            "\xe2\x9d\x8c  Non trovato in PATH  "  /* ❌ */
            "\xe2\x80\x94 <a href='https://www.danielgm.net/cc/'>Scarica</a>"
            "</span>");
        m_ccStatusLbl->setOpenExternalLinks(true);
    } else {
        m_ccStatusLbl->setText(
            "<span style='color:#4ade80;'>\xe2\x9c\x85  " + detected + "</span>");
    }
    pathLay->addWidget(m_ccPathEdit, 1);
    pathLay->addWidget(m_ccStatusLbl);
    lay->addWidget(pathGroup);

    /* Azioni principali */
    auto* actGroup = new QGroupBox(tr("\xf0\x9f\x9a\x80  Azioni"), w);
    auto* actLay   = new QHBoxLayout(actGroup);
    actLay->setSpacing(8);

    auto* btnLaunch  = new QPushButton(tr("\xf0\x9f\x94\xb5  Avvia CloudCompare"), actGroup);
    auto* btnOpenFile = new QPushButton(tr("\xf0\x9f\x93\x82  Apri file 3D\xe2\x80\xa6"), actGroup);
    auto* btnAiScript = new QPushButton(tr("\xf0\x9f\xa4\x96  Genera script Open3D"), actGroup);
    auto* btnHelp    = new QPushButton(tr("\xf0\x9f\x9b\x9f  Guida install."), actGroup);

    for (auto* b : {btnLaunch, btnOpenFile, btnAiScript, btnHelp})
        b->setObjectName("actionBtn");

    actLay->addWidget(btnLaunch);
    actLay->addWidget(btnOpenFile);
    actLay->addWidget(btnAiScript);
    actLay->addStretch();
    actLay->addWidget(btnHelp);
    lay->addWidget(actGroup);

    connect(btnLaunch,   &QPushButton::clicked, this, &AppControllerPage::onCcLaunchClicked);
    connect(btnOpenFile, &QPushButton::clicked, this, &AppControllerPage::onCcOpenFileClicked);
    connect(btnAiScript, &QPushButton::clicked, this, &AppControllerPage::onCcAiScriptClicked);
    connect(btnHelp,     &QPushButton::clicked, this, &AppControllerPage::onCcHelpClicked);

    /* Output log */
    m_ccOutput = new QTextEdit(w);
    m_ccOutput->setReadOnly(true);
    m_ccOutput->setObjectName("outputView");
    m_ccOutput->setPlaceholderText(tr("Output CloudCompare / script Open3D apparirà qui\xe2\x80\xa6"));
    lay->addWidget(m_ccOutput, 1);

    return w;
}


/* ======================================================================
   Sezione 6 — CloudCompare tab slots
   ====================================================================== */

void AppControllerPage::onCcHelpClicked()
{
    auto* dlg = new QDialog(this);
    dlg->setWindowTitle(tr("\xf0\x9f\x94\xb5  Installazione CloudComPy"));
    dlg->setAttribute(Qt::WA_DeleteOnClose);
    dlg->resize(560, 460);
    auto* dlay    = new QVBoxLayout(dlg);
    auto* browser = new QTextBrowser(dlg);
    browser->setOpenExternalLinks(true);
    browser->setHtml(
        "<h3>\xf0\x9f\x94\xb5 CloudCompare + CloudComPy</h3>"
        "<h4>1. Installa CloudCompare</h4>"
        "<p><code>sudo apt install cloudcompare</code> oppure "
        "<a href='https://www.danielgm.net/cc/'>danielgm.net/cc</a></p>"
        "<h4>2. Installa CloudComPy (Python wrapper)</h4>"
        "<pre>pip install cloudcompy</pre>"
        "<p>Oppure compila dal sorgente: "
        "<a href='https://github.com/CloudCompare/CloudComPy'>"
        "github.com/CloudCompare/CloudComPy</a></p>"
        "<h4>3. Formati supportati</h4>"
        "<p>LAS \xc2\xb7 PLY \xc2\xb7 E57 \xc2\xb7 PCD \xc2\xb7 BIN (formato nativo CC)</p>"
        "<h4>Stato attuale</h4>"
        "<p>\xe2\x8f\xb3 Il bridge \xc3\xa8 in fase di integrazione in Prismalux. "
        "Questa scheda sar\xc3\xa0 abilitata in una versione futura.</p>");
    auto* btnClose = new QPushButton(tr("\xe2\x9c\x95  Chiudi"), dlg);
    btnClose->setObjectName("actionBtn");
    connect(btnClose, &QPushButton::clicked, dlg, &QDialog::accept);
    dlay->addWidget(browser);
    dlay->addWidget(btnClose);
    dlg->exec();
}

void AppControllerPage::onCcLaunchClicked()
{
    const QString exe = m_ccPathEdit ? m_ccPathEdit->text().trimmed() : "cloudcompare";
    const QString resolved = exe.isEmpty()
        ? QStandardPaths::findExecutable("cloudcompare") : exe;

    if (resolved.isEmpty()) {
        m_ccOutput->append(
            "\xe2\x9d\x8c  CloudCompare non trovato. "  /* ❌ */
            "Installa con: sudo apt install cloudcompare");
        return;
    }
    if (!QProcess::startDetached(resolved, {}))
        m_ccOutput->append("\xe2\x9d\x8c  Impossibile avviare CloudCompare: " + resolved);
    else
        m_ccOutput->append("\xf0\x9f\x94\xb5  CloudCompare avviato: " + resolved);
}

void AppControllerPage::onCcOpenFileClicked()
{
    const QString exe = m_ccPathEdit ? m_ccPathEdit->text().trimmed() : "cloudcompare";
    const QString resolved = exe.isEmpty()
        ? QStandardPaths::findExecutable("cloudcompare") : exe;

    const QString path = QFileDialog::getOpenFileName(this,
        tr("Apri file 3D"), QDir::homePath(),
        tr("Nuvole di punti (*.las *.laz *.ply *.pcd *.e57 *.bin *.xyz *.csv);;Tutti i file (*)"));
    if (path.isEmpty()) return;

    m_ccOutput->append("\xf0\x9f\x93\x82  Apertura: " + path);

    if (resolved.isEmpty()) {
        /* CloudCompare non installato — mostra info sul file */
        QFileInfo fi(path);
        m_ccOutput->append(QString(
            "\xe2\x84\xb9  File: %1\n  Formato: %2\n  Dimensione: %3 KB\n\n"
            "CloudCompare non trovato. Genera uno script Open3D con il pulsante accanto.")
            .arg(fi.fileName(), fi.suffix().toUpper())
            .arg(fi.size() / 1024));
        return;
    }
    if (!QProcess::startDetached(resolved, {path}))
        m_ccOutput->append("\xe2\x9d\x8c  Impossibile aprire il file con CloudCompare.");
}

void AppControllerPage::onCcAiScriptClicked()
{
    const QString path = QFileDialog::getOpenFileName(this,
        tr("Seleziona file 3D"), QDir::homePath(),
        tr("Nuvole di punti (*.las *.laz *.ply *.pcd *.e57 *.xyz);;Tutti i file (*)"));
    if (path.isEmpty()) return;

    QFileInfo fi(path);
    const QString fmt = fi.suffix().toLower();
    const QString loadFn = (fmt == "las" || fmt == "laz")
        ? "o3d.t.io.read_point_cloud"   /* Open3D legacy las non è supportato direttamente */
        : "o3d.io.read_point_cloud";

    const QString script =
        "#!/usr/bin/env python3\n"
        "# Script Open3D generato da Prismalux\n"
        "# File: " + path + "\n\n"
        "import open3d as o3d\nimport numpy as np\n\n"
        "# Carica la nuvola di punti\n"
        "pcd = " + loadFn + "(\"" + path + "\")\n"
        "print(f\"Punti totali: {len(pcd.points)}\")\n\n"
        "# Stima normali\n"
        "pcd.estimate_normals(search_param=o3d.geometry.KDTreeSearchParamHybrid(\n"
        "    radius=0.1, max_nn=30))\n\n"
        "# Filtraggio statistico outlier\n"
        "cl, ind = pcd.remove_statistical_outlier(nb_neighbors=20, std_ratio=2.0)\n"
        "pcd_clean = pcd.select_by_index(ind)\n"
        "print(f\"Punti dopo pulizia: {len(pcd_clean.points)}\")\n\n"
        "# Visualizza\n"
        "o3d.visualization.draw_geometries([pcd_clean],\n"
        "    window_name=\"" + fi.fileName() + " — Prismalux\",\n"
        "    width=1024, height=768)\n";

    m_ccOutput->setPlainText(script);
    m_ccOutput->append(
        "\n# Esegui con: python3 script.py\n"
        "# Requisiti: pip install open3d\n");
}

void AppControllerPage::onCcProcError(QProcess::ProcessError err)
{
    if (err == QProcess::FailedToStart)
        m_ccOutput->append("\xe2\x9d\x8c  CloudCompare non avviabile — verifica percorso.");
    if (m_ccProc) { m_ccProc->deleteLater(); m_ccProc = nullptr; }
}

void AppControllerPage::onCcProcFinished(int exitCode, QProcess::ExitStatus)
{
    if (m_ccProc) {
        m_ccOutput->append(m_ccProc->readAllStandardOutput());
        m_ccProc->deleteLater();
        m_ccProc = nullptr;
    }
    if (exitCode != 0)
        m_ccOutput->append(QString("\xe2\x9d\x8c  Uscita con codice %1").arg(exitCode));
}

