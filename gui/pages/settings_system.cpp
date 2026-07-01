#include "settings_main.h"
#include "../dpi_utils.h"
#include "../widgets/toggle_switch.h"
#include "../widgets/stt_whisper.h"
#include "../widgets/lazy_tab_loader.h"
#include "main_customize.h"
#include "main_maintenance.h"
#include "main_graph.h"
#include "../theme_manager.h"
#include "../prismalux_paths.h"
namespace P = PrismaluxPaths;
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QFormLayout>
#include <QTabWidget>
#include <QScrollArea>
#include <QLabel>
#include <QPushButton>
#include <QFrame>
#include <QButtonGroup>
#include <QLineEdit>
#include <QComboBox>
#include <QSpinBox>
#include <QDoubleSpinBox>
#include <QSlider>
#include <QSettings>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QProcess>
#include <QFileDialog>
#include <QDateTime>
#include <QStandardPaths>
#include <QDesktopServices>
#include <QUrl>
#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <functional>
#include <QCoreApplication>
#include <QTimer>
#include <QRadioButton>
#include <QCheckBox>
#include <QListWidget>
#include <QTableWidget>
#include <QHeaderView>
#include <QGroupBox>
#include <QPlainTextEdit>
#include <QThread>
#include <QStackedWidget>
#include <QColorDialog>
#include <QPainter>
#include <QPen>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QGuiApplication>
#include <QClipboard>
#include <QMessageBox>
#include <QProgressBar>
#include <QTextEdit>
#include <QTextBrowser>
#include <QDialogButtonBox>
#include <QDialog>
#include <QRegularExpression>

/* ── Free functions: logica backup e audit ───────────────────── */
static void sysDoBackup(const QStringList& files, QLabel* statusLbl)
{
    const QString ts = QDateTime::currentDateTime().toString("yyyy-MM-dd_HH-mm-ss");
    const QString defName = QString("prismalux_backup_%1").arg(ts);
    const QString destDir = QFileDialog::getExistingDirectory(
        nullptr, "Scegli cartella di destinazione backup", QDir::homePath());
    if (destDir.isEmpty()) return;
    int copied = 0;
    QStringList failed;
    for (const QString& src : files) {
        if (!QFile::exists(src)) continue;
        const QFileInfo fi(src);
        const QString dst = destDir + "/" + defName + "_" + fi.fileName();
        if (QFile::copy(src, dst))
            ++copied;
        else
            failed << fi.fileName();
    }
    if (copied > 0 && failed.isEmpty())
        statusLbl->setText(
            QString("\xe2\x9c\x85  %1 file copiati in %2/")
                .arg(copied).arg(QDir(destDir).dirName()));
    else if (!failed.isEmpty())
        statusLbl->setText(
            QString("\xe2\x9a\xa0\xef\xb8\x8f  %1 copiati, errori: %2")
                .arg(copied).arg(failed.join(", ")));
    else
        statusLbl->setText("\xe2\x9a\xa0\xef\xb8\x8f  Nessun file trovato da copiare.");
}

static void sysDoAudit(QTextEdit* auditLog)
{
    auditLog->clear();
    int pass = 0, warn = 0;

    auto ok  = [&](const QString& msg){ auditLog->append("\xe2\x9c\x85 " + msg); ++pass; };
    auto bad = [&](const QString& msg){ auditLog->append("\xe2\x9d\x8c " + msg); ++warn; };
    auto info= [&](const QString& msg){ auditLog->append("\xe2\x84\xb9\xef\xb8\x8f " + msg); };

#ifdef PRISMALUX_ROOT
    const QString root = QString(PRISMALUX_ROOT);
#else
    const QString root = P::root();
#endif

    /* 1. .env in .gitignore */
    const QString gitignore = root + "/.gitignore";
    if (QFile::exists(gitignore)) {
        QFile f(gitignore);
        if (f.open(QIODevice::ReadOnly)) {
            const QString content = QString::fromUtf8(f.readAll());
            if (content.contains(".env"))
                ok(".env presente in .gitignore");
            else
                bad(".env NON trovato in .gitignore — aggiungilo!");
            if (content.contains("*.key") || content.contains("lan_token"))
                ok("File *.key / token LAN in .gitignore");
            else
                bad("*.key non in .gitignore — aggiungere '*.key'");
        }
    } else {
        bad(".gitignore non trovato in " + root);
    }

    /* 2. Permessi file sensibili (solo Linux) */
#ifndef Q_OS_WIN
    const QStringList sensitiveFiles = {
        QDir::homePath() + "/.prismalux/lan_token.key",
        QDir::homePath() + "/.prismalux/graph_memory.db",
        QDir::homePath() + "/.prismalux/rag_graph.db",
    };
    for (const QString& path : sensitiveFiles) {
        if (!QFile::exists(path)) continue;
        QFileInfo fi(path);
        const QFile::Permissions p = fi.permissions();
        const bool groupRead = p & (QFile::ReadGroup | QFile::WriteGroup | QFile::ExeGroup);
        const bool otherRead = p & (QFile::ReadOther | QFile::WriteOther | QFile::ExeOther);
        if (!groupRead && !otherRead)
            ok("Permessi 0600 su " + fi.fileName());
        else
            bad("Permessi troppo aperti su " + fi.fileName() + " — esegui: chmod 600 " + path);
    }
#else
    info("Verifica permessi file non disponibile su Windows");
#endif

    /* 3. Nessun token LAN in QSettings in chiaro */
    QSettings s("Prismalux", "GUI");
    if (s.contains("lan/token") || s.contains("lan_token") || s.contains("token")) {
        const QString val = s.value("lan/token",
            s.value("lan_token", s.value("token"))).toString();
        if (!val.isEmpty() && val.length() > 8)
            bad("Token LAN trovato in QSettings — usa QKeychain per sicurezza");
        else
            ok("Nessun token LAN in chiaro in QSettings");
    } else {
        ok("Nessun token LAN in chiaro in QSettings");
    }

    /* 4. KNOWLEDGE_USER non in git */
    const QString kuDir = root + "/KNOWLEDGE_USER";
    if (QDir(kuDir).exists()) {
        if (QFile::exists(gitignore)) {
            QFile f(gitignore);
            if (f.open(QIODevice::ReadOnly)) {
                if (QString::fromUtf8(f.readAll()).contains("KNOWLEDGE_USER"))
                    ok("KNOWLEDGE_USER in .gitignore");
                else
                    bad("KNOWLEDGE_USER NON in .gitignore — dati privati esposti!");
            }
        }
    } else {
        info("KNOWLEDGE_USER non trovata — skip");
    }

    /* 5. RAG/ non in git */
    const QString ragDir = root + "/RAG";
    if (QDir(ragDir).exists()) {
        if (QFile::exists(gitignore)) {
            QFile f(gitignore);
            if (f.open(QIODevice::ReadOnly)) {
                if (QString::fromUtf8(f.readAll()).contains("RAG/") ||
                    QString::fromUtf8(f.readAll()).contains("/RAG"))
                    ok("RAG/ in .gitignore");
                else
                    bad("RAG/ NON in .gitignore — documenti privati esposti!");
            }
        }
    }

    auditLog->append(QString("\n\xe2\x80\x94\xe2\x80\x94 %1 controlli OK, %2 avvisi \xe2\x80\x94\xe2\x80\x94")
        .arg(pass).arg(warn));
}

/* ══════════════════════════════════════════════════════════════
   buildGroupSistema — Gruppo esterno "🔧 Sistema": Pulizia (eager)
   · Bug LLMs · Memoria AI · Consigli (lazy — costruite al primo clic).
   Chiamata da LazyTabLoader al primo clic sulla tab esterna "Sistema".
   NOTA: buildPuliziaTab() fa una scansione disco sincrona (calcFilesSize,
   QDirIterator ricorsivo) — non più sul percorso critico dell'apertura
   di Impostazioni (differita fino al clic su "Sistema"), ma resta un
   candidato a diventare asincrona in futuro (vedi CLAUDE.md).
   ══════════════════════════════════════════════════════════════ */
QWidget* ImpostazioniPage::buildGroupSistema()
{
    auto* t = new QTabWidget;
    t->setObjectName("settingsInnerTabs");
    t->setDocumentMode(true);
    t->setUsesScrollButtons(true);
    m_tabSistema = t;

    auto* inner = new LazyTabLoader(t, this);

    {
        auto* sc = new QScrollArea;
        sc->setWidgetResizable(true);
        sc->setFrameShape(QFrame::NoFrame);
        sc->setWidget(buildPuliziaTab());
        inner->addEager("\xf0\x9f\xa7\xb9  Pulizia", sc);
    }

    inner->addLazy("\xf0\x9f\x94\x8d  Bug LLMs", [this]{
        return m_manutenzione->buildBugTracker();
    });

    inner->addLazy("\xf0\x9f\xa7\xa0  Memoria AI", [this]{
        auto* sc = new QScrollArea;
        sc->setWidgetResizable(true);
        sc->setFrameShape(QFrame::NoFrame);
        sc->setWidget(buildAiMemoryTab());
        return sc;
    });

    inner->addLazy("\xf0\x9f\x8c\xa1\xef\xb8\x8f  Consigli", [this]{
        auto* sc = new QScrollArea;
        sc->setWidgetResizable(true);
        sc->setFrameShape(QFrame::NoFrame);
        sc->setWidget(buildSistemaConsigliTab());
        return sc;
    });

    return t;
}

QWidget* ImpostazioniPage::buildPuliziaTab()
{
    auto* w   = new QWidget(this);
    auto* lay = new QVBoxLayout(w);
    lay->setContentsMargins(16, 16, 16, 16);
    lay->setSpacing(10);

    auto* ttl = new QLabel("\xf0\x9f\xa7\xb9  Pulizia spazio su disco", w);
    ttl->setObjectName("cardTitle");
    lay->addWidget(ttl);

    auto* intro = new QLabel(
        "Rimuovi file temporanei, output vecchi e artefatti di build "
        "che non sono pi\xc3\xb9 necessari. "
        "Ogni sezione mostra la dimensione stimata prima di procedere.", w);
    intro->setObjectName("cardDesc");
    intro->setWordWrap(true);
    lay->addWidget(intro);

    /* ── Helper: dimensione totale file in una cartella (ricorsiva) ── */
    auto calcFilesSize = [](const QString& dirPath,
                            const QStringList& nameFilters = {}) -> QPair<qint64, int> {
        qint64 bytes = 0;
        int    count = 0;
        QDirIterator it(dirPath,
                        nameFilters.isEmpty() ? QStringList{"*"} : nameFilters,
                        QDir::Files | QDir::Hidden,
                        QDirIterator::Subdirectories);
        while (it.hasNext()) {
            it.next();
            bytes += it.fileInfo().size();
            ++count;
        }
        return {bytes, count};
    };

    auto fmtBytes = [](qint64 b) -> QString {
        if (b <= 0)    return "0 B";
        if (b < 1024)  return QString("%1 B").arg(b);
        if (b < 1<<20) return QString("%1 KB").arg(b>>10);
        if (b < 1<<30) return QString("%1 MB").arg(b>>20);
        return QString("%1 GB").arg(b>>30);
    };

    /* ── Layout a 2 colonne: sinistra griglia 2×2 + destra 3 card ── */
    auto* mainRow    = new QWidget(w);
    auto* mainRowLay = new QHBoxLayout(mainRow);
    mainRowLay->setContentsMargins(0, 0, 0, 0);
    mainRowLay->setSpacing(12);

    auto* gridW   = new QWidget(mainRow);
    auto* gridLay = new QGridLayout(gridW);
    gridLay->setContentsMargins(0, 0, 0, 0);
    gridLay->setSpacing(10);
    gridLay->setAlignment(Qt::AlignTop);
    gridLay->setColumnStretch(0, 1);
    gridLay->setColumnStretch(1, 1);
    gridLay->setRowStretch(0, 1);
    gridLay->setRowStretch(1, 1);

    auto* rightW   = new QWidget(mainRow);
    auto* rightLay = new QVBoxLayout(rightW);
    rightLay->setContentsMargins(0, 0, 0, 0);
    rightLay->setSpacing(10);
    rightLay->setAlignment(Qt::AlignTop);

    /* ── Helper: crea una card di pulizia ── */
    auto makeCard = [&](const QString& icon,
                        const QString& heading,
                        const QString& desc,
                        const QString& pathHint,
                        const QString& sizeText) -> QGroupBox* {
        auto* grp = new QGroupBox(w);
        grp->setObjectName("cardGroup");
        grp->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::MinimumExpanding);
        auto* gl  = new QVBoxLayout(grp);
        gl->setContentsMargins(12, 12, 12, 8);
        gl->setSpacing(4);

        auto* hdr = new QLabel(icon + "  " + heading, grp);
        hdr->setObjectName("cardTitle");
        gl->addWidget(hdr);

        auto* dsc = new QLabel(desc, grp);
        dsc->setObjectName("cardDesc");
        dsc->setWordWrap(true);
        dsc->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::MinimumExpanding);
        gl->addWidget(dsc);

        auto* pth = new QLabel(pathHint, grp);
        pth->setObjectName("hintLabel");
        pth->setWordWrap(true);
        pth->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::MinimumExpanding);
        gl->addWidget(pth);

        auto* row = new QWidget(grp);
        auto* rl  = new QHBoxLayout(row);
        rl->setContentsMargins(0, 4, 0, 0);
        rl->setSpacing(8);

        auto* sizeLbl   = new QLabel(sizeText, row);
        sizeLbl->setObjectName("cardDesc");
        rl->addWidget(sizeLbl, 1);

        auto* statusLbl = new QLabel("", row);
        statusLbl->setObjectName("hintLabel");
        rl->addWidget(statusLbl, 1);

        auto* btn = new QPushButton("\xf0\x9f\x97\x91  Pulisci", row);
        btn->setObjectName("actionBtn");
        btn->setProperty("danger", "true");
        btn->setFixedWidth(dpiScale(110));
        rl->addWidget(btn);

        gl->addWidget(row);

        /* Salva i puntatori come proprietà della card per accesso nei lambda */
        grp->setProperty("_sizeLbl",   QVariant::fromValue<QObject*>(sizeLbl));
        grp->setProperty("_statusLbl", QVariant::fromValue<QObject*>(statusLbl));
        grp->setProperty("_btn",       QVariant::fromValue<QObject*>(btn));
        return grp;
    };

    /* ──────────────────────────────────────────────────────────
       SEZIONE 1: Esportazioni
       ────────────────────────────────────────────────────────── */
    const QString esportDir = QDir::cleanPath(P::root() + "/../esportazioni");
    {
        auto [bytes, cnt] = calcFilesSize(esportDir);
        auto* grp = makeCard(
            "\xf0\x9f\x93\xa6",
            "Esportazioni",
            "Output del multi-agente TUI: file .txt, .py, .pyx, .html, .json generati nelle sessioni precedenti.",
            QString("Percorso: %1").arg(esportDir),
            QString("%1 file \xc2\xb7 %2").arg(cnt).arg(fmtBytes(bytes)));
        lay->addWidget(grp);

        auto* btn       = qobject_cast<QPushButton*>(grp->property("_btn").value<QObject*>());
        auto* statusLbl = qobject_cast<QLabel*>(grp->property("_statusLbl").value<QObject*>());
        auto* sizeLbl   = qobject_cast<QLabel*>(grp->property("_sizeLbl").value<QObject*>());

        connect(btn, &QPushButton::clicked, grp,
                [btn, statusLbl, sizeLbl, esportDir, calcFilesSize, fmtBytes]() {
            int removed = 0;
            QDirIterator it(esportDir, QDir::Files | QDir::Hidden);
            while (it.hasNext()) {
                it.next();
                if (QFile::remove(it.filePath())) ++removed;
            }
            statusLbl->setText(QString("\xe2\x9c\x85  %1 file rimossi").arg(removed));
            btn->setEnabled(false);
            auto [b2, c2] = calcFilesSize(esportDir);
            sizeLbl->setText(QString("%1 file \xc2\xb7 %2").arg(c2).arg(fmtBytes(b2)));
        });
        gridLay->addWidget(grp, 0, 0);
    }

    /* ──────────────────────────────────────────────────────────
       SEZIONE 2: Artefatti build Qt
       ────────────────────────────────────────────────────────── */
    {
        const QString qtGuiDir = QDir::cleanPath(P::root() + "/Qt_GUI");
        const QStringList buildDirs = {
            qtGuiDir + "/build",
            qtGuiDir + "/build_tests",
            qtGuiDir + "/build_debug",
            qtGuiDir + "/build_check",
            qtGuiDir + "/build_dbg",
        };
        qint64 totalBytes = 0;
        int    existCount = 0;
        for (const QString& d : buildDirs) {
            if (!QDir(d).exists()) continue;
            ++existCount;
            auto [b, _c] = calcFilesSize(d);
            totalBytes += b;
        }
        auto* grp = makeCard(
            "\xf0\x9f\x94\xa8",
            "Artefatti build Qt",
            "Cartelle generate da cmake: build/, build_tests/, build_debug/, build_check/, build_dbg/. "
            "Ricrea con: cmake -B build && cmake --build build",
            QString("Percorso: %1/build*/").arg(qtGuiDir),
            QString("%1 cartell%2 \xc2\xb7 %3")
                .arg(existCount)
                .arg(existCount == 1 ? "a" : "e")
                .arg(fmtBytes(totalBytes)));
        lay->addWidget(grp);

        auto* btn       = qobject_cast<QPushButton*>(grp->property("_btn").value<QObject*>());
        auto* statusLbl = qobject_cast<QLabel*>(grp->property("_statusLbl").value<QObject*>());
        auto* sizeLbl   = qobject_cast<QLabel*>(grp->property("_sizeLbl").value<QObject*>());

        connect(btn, &QPushButton::clicked, grp,
                [btn, statusLbl, sizeLbl, buildDirs, fmtBytes]() {
            int removed = 0;
            for (const QString& d : buildDirs)
                if (QDir(d).exists() && QDir(d).removeRecursively()) ++removed;
            statusLbl->setText(QString("\xe2\x9c\x85  %1 cartell%2 rimosse")
                               .arg(removed).arg(removed == 1 ? "a" : "e"));
            btn->setEnabled(false);
            sizeLbl->setText(tr("0 cartelle \xc2\xb7 0 B"));
        });
        gridLay->addWidget(grp, 0, 1);
    }

    /* ──────────────────────────────────────────────────────────
       SEZIONE 3: Cache Python (__pycache__)
       ────────────────────────────────────────────────────────── */
    {
        const QString pyDir = QDir::cleanPath(P::root() + "/../Python_Software");
        int    cacheCount = 0;
        qint64 cacheBytes = 0;
        {
            QDirIterator it(pyDir, QStringList{"__pycache__"},
                            QDir::Dirs | QDir::NoDotAndDotDot,
                            QDirIterator::Subdirectories);
            while (it.hasNext()) {
                it.next();
                ++cacheCount;
                auto [b, _c] = calcFilesSize(it.filePath());
                cacheBytes += b;
            }
        }
        auto* grp = makeCard(
            "\xf0\x9f\x90\x8d",
            "Cache Python",
            "Cartelle __pycache__ generate dall'interprete Python in Python_Software/. "
            "Vengono ricreate automaticamente alla prossima esecuzione.",
            QString("Percorso: %1/*/__pycache__/").arg(pyDir),
            QString("%1 cartell%2 \xc2\xb7 %3")
                .arg(cacheCount)
                .arg(cacheCount == 1 ? "a" : "e")
                .arg(fmtBytes(cacheBytes)));
        lay->addWidget(grp);

        auto* btn       = qobject_cast<QPushButton*>(grp->property("_btn").value<QObject*>());
        auto* statusLbl = qobject_cast<QLabel*>(grp->property("_statusLbl").value<QObject*>());
        auto* sizeLbl   = qobject_cast<QLabel*>(grp->property("_sizeLbl").value<QObject*>());

        connect(btn, &QPushButton::clicked, grp,
                [btn, statusLbl, sizeLbl, pyDir]() {
            int removed = 0;
            QStringList toRemove;
            QDirIterator it(pyDir, QStringList{"__pycache__"},
                            QDir::Dirs | QDir::NoDotAndDotDot,
                            QDirIterator::Subdirectories);
            while (it.hasNext()) { it.next(); toRemove << it.filePath(); }
            for (const QString& d : toRemove)
                if (QDir(d).removeRecursively()) ++removed;
            statusLbl->setText(QString("\xe2\x9c\x85  %1 cartell%2 rimosse")
                               .arg(removed).arg(removed == 1 ? "a" : "e"));
            btn->setEnabled(false);
            sizeLbl->setText(tr("0 cartelle \xc2\xb7 0 B"));
        });
        gridLay->addWidget(grp, 1, 0);
    }

    /* ──────────────────────────────────────────────────────────
       SEZIONE 4: File temporanei editor (prismalux_code.*)
       ────────────────────────────────────────────────────────── */
    {
        const QString tmpDir = QStandardPaths::writableLocation(QStandardPaths::TempLocation);
        auto [bytes, cnt] = calcFilesSize(tmpDir, {"prismalux_code*"});
        auto* grp = makeCard(
            "\xf0\x9f\x93\x84",
            "File temporanei editor",
            "File prismalux_code.* e prismalux_code_bin lasciati dalla pagina Programmazione "
            "dopo ogni esecuzione di codice.",
            QString("Percorso: %1/prismalux_code*").arg(tmpDir),
            QString("%1 file \xc2\xb7 %2").arg(cnt).arg(fmtBytes(bytes)));
        lay->addWidget(grp);

        auto* btn       = qobject_cast<QPushButton*>(grp->property("_btn").value<QObject*>());
        auto* statusLbl = qobject_cast<QLabel*>(grp->property("_statusLbl").value<QObject*>());
        auto* sizeLbl   = qobject_cast<QLabel*>(grp->property("_sizeLbl").value<QObject*>());

        connect(btn, &QPushButton::clicked, grp,
                [btn, statusLbl, sizeLbl, tmpDir]() {
            int removed = 0;
            QDirIterator it(tmpDir, QStringList{"prismalux_code*"}, QDir::Files);
            while (it.hasNext()) {
                it.next();
                if (QFile::remove(it.filePath())) ++removed;
            }
            statusLbl->setText(QString("\xe2\x9c\x85  %1 file rimossi").arg(removed));
            btn->setEnabled(false);
            sizeLbl->setText(tr("0 file \xc2\xb7 0 B"));
        });
        gridLay->addWidget(grp, 1, 1);
    }

    /* ──────────────────────────────────────────────────────────
       SEZIONE 5: Cartelle pesanti (solo informazione)
       ────────────────────────────────────────────────────────── */
    {
        auto* grp = new QGroupBox(w);
        grp->setObjectName("cardGroup");
        auto* gl  = new QVBoxLayout(grp);
        gl->setContentsMargins(12, 12, 12, 12);
        gl->setSpacing(6);

        auto* hdr = new QLabel(
            "\xe2\x9a\xa0\xef\xb8\x8f  Cartelle pesanti (rimozione manuale)", grp);
        hdr->setObjectName("cardTitle");
        gl->addWidget(hdr);

        const QList<QPair<QString,QString>> heavy = {
            { "C_software/llama_cpp_studio/",
              QDir::cleanPath(P::llamaStudioDir()) },
        };
        bool anyFound = false;
        for (const auto& [label, path] : heavy) {
            if (!QDir(path).exists()) continue;
            anyFound = true;
            auto [b, _c] = calcFilesSize(path);
            auto* lbl = new QLabel(
                QString("\xf0\x9f\x93\x81  <b>%1</b>  \xe2\x80\x94  %2  \xe2\x80\x94  sorgenti llama.cpp (git clone)")
                    .arg(label).arg(fmtBytes(b)),
                grp);
            lbl->setObjectName("cardDesc");
            lbl->setTextFormat(Qt::RichText);
            lbl->setWordWrap(true);
            gl->addWidget(lbl);
        }
        if (!anyFound) {
            auto* lbl = new QLabel("\xe2\x9c\x85  Nessuna cartella llama_cpp_studio trovata.", grp);
            lbl->setObjectName("hintLabel");
            gl->addWidget(lbl);
        }
        auto* note = new QLabel(
            "Necessarie per compilare llama-server localmente (tab Personalizza \xe2\x86\x92 llama.cpp Studio). "
            "Rimuovi manualmente solo se non usi pi\xc3\xb9 llama.cpp locale.",
            grp);
        note->setObjectName("hintLabel");
        note->setWordWrap(true);
        gl->addWidget(note);
        rightLay->addWidget(grp);
    }

    /* ── Backup dati ── */
    {
        auto* grp = new QGroupBox(
            "\xf0\x9f\x92\xbe  Backup dati Prismalux", w);
        auto* gl  = new QVBoxLayout(grp);
        gl->setContentsMargins(12, 12, 12, 8);
        gl->setSpacing(6);

        auto* desc = new QLabel(
            "Crea un backup ZIP dei database SQLite (<b>graph_memory.db</b>, "
            "<b>rag_graph.db</b>, <b>chat_history.db</b>) e del log di accesso. "
            "I file vengono copiati nella cartella scelta con timestamp nel nome.",
            grp);
        desc->setObjectName("cardDesc");
        desc->setTextFormat(Qt::RichText);
        desc->setWordWrap(true);
        gl->addWidget(desc);

        const QString prismaDir = QDir::homePath() + "/.prismalux";
        const QStringList backupFiles = {
            prismaDir + "/graph_memory.db",
            prismaDir + "/rag_graph.db",
            prismaDir + "/chat_history.db",
            prismaDir + "/access.log",
        };
        qint64 totalBytes = 0;
        for (const QString& f : backupFiles) {
            QFileInfo fi(f);
            if (fi.exists()) totalBytes += fi.size();
        }
        auto* sizeInfo = new QLabel(
            QString("Dimensione stimata: <b>%1</b>  —  Cartella: <code>~/.prismalux/</code>")
                .arg(fmtBytes(totalBytes)),
            grp);
        sizeInfo->setObjectName("hintLabel");
        sizeInfo->setTextFormat(Qt::RichText);
        gl->addWidget(sizeInfo);

        auto* btnRow  = new QHBoxLayout;
        auto* backupBtn = new QPushButton(
            "\xf0\x9f\x92\xbe  Crea backup ZIP...", grp);
        backupBtn->setObjectName("actionBtn");
        backupBtn->setFixedWidth(dpiScale(180));
        auto* openDirBtn = new QPushButton(
            "\xf0\x9f\x93\x82  Apri cartella dati", grp);
        openDirBtn->setObjectName("actionBtn");
        openDirBtn->setFixedWidth(dpiScale(160));
        auto* statusLbl = new QLabel("", grp);
        statusLbl->setObjectName("statusLabel");
        btnRow->addWidget(backupBtn);
        btnRow->addWidget(openDirBtn);
        btnRow->addWidget(statusLbl, 1);
        gl->addLayout(btnRow);

        connect(backupBtn, &QPushButton::clicked, grp, [backupFiles, statusLbl](){
            sysDoBackup(backupFiles, statusLbl);
        });

        connect(openDirBtn, &QPushButton::clicked, grp, [prismaDir]() {
            QDesktopServices::openUrl(QUrl::fromLocalFile(prismaDir));
        });

        rightLay->addWidget(grp);
    }

    /* ── Audit segreti ── */
    {
        auto* grp = new QGroupBox(
            "\xf0\x9f\x94\x90  Audit segreti & permessi", w);
        auto* gl  = new QVBoxLayout(grp);
        gl->setContentsMargins(12, 12, 12, 8);
        gl->setSpacing(6);

        auto* desc = new QLabel(
            "Verifica che file sensibili non siano esposti: "
            "<code>.env</code> in <code>.gitignore</code>, "
            "permessi <code>0600</code> su token LAN, "
            "nessun segreto in chiaro nei settings Qt.",
            grp);
        desc->setObjectName("cardDesc");
        desc->setTextFormat(Qt::RichText);
        desc->setWordWrap(true);
        gl->addWidget(desc);

        auto* auditBtn = new QPushButton(
            "\xf0\x9f\x94\x8d  Esegui audit ora", grp);
        auditBtn->setObjectName("actionBtn");
        auditBtn->setFixedWidth(dpiScale(160));
        auto* auditLog = new QTextEdit(grp);
        auditLog->setReadOnly(true);
        auditLog->setFixedHeight(dpiScale(150));
        auditLog->setPlaceholderText(tr("Risultati audit..."));
        gl->addWidget(auditBtn);
        gl->addWidget(auditLog);

        connect(auditBtn, &QPushButton::clicked, grp, [auditLog](){
            sysDoAudit(auditLog);
        });

        rightLay->addWidget(grp);
        rightLay->addStretch();
    }

    mainRowLay->addWidget(gridW, 1);
    mainRowLay->addWidget(rightW, 1);
    lay->addWidget(mainRow, 1);
    return w;
}

/* ══════════════════════════════════════════════════════════════
   buildSistemaConsigliTab — consigli ottimizzazione hardware e temperatura.
   ══════════════════════════════════════════════════════════════ */
QWidget* ImpostazioniPage::buildSistemaConsigliTab()
{
    auto* root = new QWidget;
    auto* vlay = new QVBoxLayout(root);
    vlay->setContentsMargins(dpiScale(16), dpiScale(16), dpiScale(16), dpiScale(16));
    vlay->setSpacing(dpiScale(14));

    /* ── Titolo ─────────────────────────────────────────────── */
    auto* title = new QLabel(
        "<h2>\xf0\x9f\x8c\xa1\xef\xb8\x8f Consigli Ottimizzazione Sistema</h2>"
        "<p style='color:gray;margin-top:2px;'>"
        "Suggerimenti per ridurre il surriscaldamento e migliorare le prestazioni del PC."
        "</p>");
    title->setWordWrap(true);
    vlay->addWidget(title);

    /* ── Helper per costruire una card consiglio ──────────────── */
    struct Tip {
        const char* icon;
        const char* titolo;
        const char* desc;
        const char* cmd;   /* nullptr = nessun comando da copiare */
    };

    static const Tip TIPS[] = {
        {
            "\xf0\x9f\xa7\xb9",
            "Pulizia fisica (causa principale)",
            "La polvere accumulata nelle griglie e sul dissipatore "
            "\xc3\xa8 la causa pi\xc3\xb9 comune di surriscaldamento sui laptop. "
            "Soffia aria compressa nelle griglie laterali/posteriori. "
            "Su un laptop con anni di utilizzo si abbassano tipicamente 15\xe2\x80\x93" "20\xc2\xb0" "C.",
            nullptr
        },
        {
            "\xf0\x9f\x92\xa4",
            "Modalit\xc3\xa0 risparmio energia CPU (powersave)",
            "Mantiene il processore a frequenze pi\xc3\xb9 basse quando non serve, "
            "producendo meno calore. Non influisce sulla reattivit\xc3\xa0 percepita nell'uso normale. "
            "Torna automaticamente alle frequenze alte quando serve. "
            "Per renderlo permanente dopo il riavvio aggiungi il comando a /etc/rc.local.",
            "sudo cpupower frequency-set -g powersave"
        },
        {
            "\xf0\x9f\x9b\xa1\xef\xb8\x8f",
            "Intel thermald (gestione termica automatica)",
            "Abbassa attivamente le frequenze prima che la temperatura superi le soglie critiche, "
            "evitando il thermal throttling brusco del BIOS. "
            "Se gi\xc3\xa0 installato, abilita il servizio permanente con il comando qui sotto.",
            "sudo systemctl enable --now thermald"
        },
        {
            "\xf0\x9f\x93\x8a",
            "Controlla le temperature in tempo reale",
            "Mostra le temperature di CPU, GPU e chipset. "
            "Valori normali a riposo: CPU 40\xe2\x80\x93" "60\xc2\xb0" "C. "
            "Valori preoccupanti: CPU >80\xc2\xb0" "C costante, ACPI >85\xc2\xb0" "C.",
            "watch -n 2 sensors"
        },
        {
            "\xf0\x9f\x94\x8d",
            "Trova processi che consumano CPU",
            "Ordina i processi per uso CPU. Se un processo occupa costantemente il 30%+ "
            "a riposo \xc3\xa8 un candidato da investigare o terminare.",
            "top -bn1 | sort -t',' -k9 -rn | head -15"
        },
    };

    for (const auto& tip : TIPS) {
        auto* card = new QFrame;
        card->setFrameShape(QFrame::StyledPanel);
        card->setContentsMargins(dpiScale(2), dpiScale(2), dpiScale(2), dpiScale(2));

        auto* cLay = new QVBoxLayout(card);
        cLay->setContentsMargins(dpiScale(14), dpiScale(12), dpiScale(14), dpiScale(12));
        cLay->setSpacing(dpiScale(6));

        auto* hdr = new QLabel(
            QString("<b>%1 %2</b>").arg(tip.icon).arg(tip.titolo));
        hdr->setWordWrap(true);
        cLay->addWidget(hdr);

        auto* desc = new QLabel(tip.desc);
        desc->setWordWrap(true);
        cLay->addWidget(desc);

        if (tip.cmd) {
            auto* cmdRow = new QHBoxLayout;
            cmdRow->setSpacing(dpiScale(8));

            auto* cmdLbl = new QLabel(
                QString("<code style='background:rgba(128,128,128,0.15);"
                        "padding:3px 8px;border-radius:4px;'>%1</code>")
                .arg(tip.cmd));
            cmdLbl->setTextFormat(Qt::RichText);
            cmdLbl->setWordWrap(true);
            cmdRow->addWidget(cmdLbl, 1);

            auto* btnCopy = new QPushButton("\xf0\x9f\x93\x8b Copia");
            btnCopy->setFixedWidth(dpiScale(90));
            const QString cmdStr = tip.cmd;
            connect(btnCopy, &QPushButton::clicked, btnCopy, [cmdStr] {
                QGuiApplication::clipboard()->setText(cmdStr);
            });
            cmdRow->addWidget(btnCopy);
            cLay->addLayout(cmdRow);
        }

        vlay->addWidget(card);
    }

    /* ── Nota finale ─────────────────────────────────────────── */
    auto* nota = new QLabel(
        "<p style='color:gray;font-size:small;'>"
        "\xf0\x9f\x92\xa1 <b>Consiglio:</b> Inizia sempre dalla pulizia fisica &mdash; "
        "\xc3\xa8 gratuita e risolve il 90% dei casi di surriscaldamento sui laptop."
        "</p>");
    nota->setWordWrap(true);
    vlay->addWidget(nota);

    vlay->addStretch();
    return root;
}

/* ══════════════════════════════════════════════════════════════
   buildRingraziamentiTab — licenza MIT + crediti autore, librerie e MCPs.
   Widget Qt nativi (nessun colore hardcodato): palette tema automatica.
   Link cliccabili via setCellWidget + QLabel HTML (QPalette::Link).
   ══════════════════════════════════════════════════════════════ */

